#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>

#define HEADER_INPUT  0xF0
#define HEADER_OUTPUT 0xF1
#define CMD_GET       0xA1
#define CMD_SET       0xB1
#define CMD_XXX_176   0xB0
#define CMD_XXX_193   0xC1

#define VOLTAGE_SET 193
#define CURRENT_SET 194
#define VOLTAGE_CURRENT_POWER_GET 195
#define OUTPUT_ENABLE 219
#define MODEL_NAME 222
#define HARDWARE_VERSION 223
#define FIRMWARE_VERSION 224
#define METERING_ENABLE 216
#define OVP 209
#define OCP 210

int serial_fd;
char *device = "/dev/ttyUSB0";
int debug;	// Debug flag

/* Opens the serial port */
int open_serial(const char *device) {
    struct termios options;
    serial_fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd == -1) {
        perror("Error opening serial port");
        return -1;
    }
    tcgetattr(serial_fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag = CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    tcflush(serial_fd, TCIFLUSH);
    tcsetattr(serial_fd, TCSANOW, &options);
    return 0;
}

/* Send command */
void send_command(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t *c5, uint8_t c4) {
    uint8_t c6 = c3 + c4;
    for (int i = 0; i < c4; i++) {
        c6 += c5[i];
    }
    uint8_t command[c4 + 5];
    command[0] = c1;
    command[1] = c2;
    command[2] = c3;
    command[3] = c4;
    memcpy(&command[4], c5, c4);
    command[4 + c4] = c6;
    write(serial_fd, command, sizeof(command));
    usleep(50000);
}

/* Parse response */
void receive_response(int response_type) {
    uint8_t buffer[1024];
    int bytes_read = read(serial_fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
	/* Print raw data for debugging */
	if (debug) {
    	    printf("Received: ");
    	    for (int i = 0; i < bytes_read; i++) {
        	printf("%02X (%03d) ", buffer[i], buffer[i]);
    	    }
    	    printf("\n");
        }
        if (bytes_read > 6 && buffer[0] == HEADER_INPUT && buffer[1] == CMD_GET) {
            uint8_t cmd_type = buffer[2];
            float value;
            memcpy(&value, &buffer[4], sizeof(float));
            
            switch (cmd_type) {
                case VOLTAGE_SET:
                    printf("Output Voltage: %.2fV\n", value);
                    break;
                case CURRENT_SET:
                    printf("Output Current: %.2fA\n", value);
                    break;
                case VOLTAGE_CURRENT_POWER_GET:
		    if (response_type == 0)
                	printf("Output Voltage: %.2fV\n", value);
		    if (response_type == 1) {
			memcpy(&value, &buffer[4+4], sizeof(float));
                	printf("Output Current: %.3fA\n", value);
		    }
		    if (response_type == 2) {
			memcpy(&value, &buffer[4+8], sizeof(float));
                	printf("Output Power: %.2fW\n", value);
		    }
                    break;
                case 222:
                    printf("Device Model: %s\n", (char*)&buffer[4]);
                    break;
                case 223:
                    printf("Hardware Version: %s\n", (char*)&buffer[4]);
                    break;
                case 224:
                    printf("Firmware Version: %s\n", (char*)&buffer[4]);
                    break;
                default:
                    printf("Unknown response\n");
            }
        }
    }
}

/* Send float value i.e. voltages, currents,... */
void set_float_value(uint8_t type, float value) {
    uint8_t data[4];
    memcpy(data, &value, sizeof(float));
    send_command(HEADER_OUTPUT, CMD_SET, type, data, 4);
}

/* Send byte value i.e. on/off flags */
void set_byte_value(uint8_t type, uint8_t value) {
    uint8_t data[1] = {value};
    send_command(HEADER_OUTPUT, CMD_SET, type, data, 1);
}

void enable_output() {
    set_byte_value(OUTPUT_ENABLE, 1);
}

void disable_output() {
    set_byte_value(OUTPUT_ENABLE, 0);
}

/* Enable or disable over-voltage protection */
void set_ovp(uint8_t state) {
    set_byte_value(OVP, state);
}

/* Enable or disable over-current protection */
void set_ocp(uint8_t state) {
    set_byte_value(OCP, state);
}

void get_model_name() {
    send_command(HEADER_OUTPUT, CMD_GET, MODEL_NAME, 0, 0);
}

/* Initialize the power supply communition */
void init_device() {
    uint8_t baudrate_index = 4; // 115200 baud
    send_command(HEADER_OUTPUT, CMD_XXX_193, 0, (uint8_t[]){1}, 1);
    send_command(HEADER_OUTPUT, CMD_XXX_176, 0, &baudrate_index, 1);
}

/* Close the serial port */
void close_serial(int disconnect) {
    if (disconnect) {
        send_command(HEADER_OUTPUT, CMD_XXX_193, 0, NULL, 0);
    }

    close(serial_fd);
}


int main(int argc, char *argv[]) {
    float voltage = -1.0, current = -1.0;
    int output = -1, ovp = -1, ocp = -1, get_voltage = 0, get_current = 0, get_power = 0, get_info = 0;
    int opt;
    int disconnect = 1;
    debug=0;

    while ((opt = getopt(argc, argv, "d:u:i:x:y:UIPVo:zv")) != -1) {
        switch (opt) {
            case 'd': device = optarg; break;
            case 'u': voltage = atof(optarg); break;
            case 'i': current = atof(optarg); break;
            case 'x': ovp = atoi(optarg); break;
            case 'y': ocp = atoi(optarg); break;
            case 'U': get_voltage = 1; break;
            case 'I': get_current = 1; break;
	    case 'P': get_power = 1; break;
            case 'V': get_info = 1; break;
            case 'o': output = atoi(optarg); break;
	    case 'z': disconnect = 0; break;
	    case 'v': debug = 1; break;
            default:
                fprintf(stderr, "Usage: %s [-d device] [-u voltage] [-i current] [-x 0|1] [-y 0|1] [-U] [-I] [-P] [-V] [-o 0|1] [-z] [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (open_serial(device) != 0) {
        return 1;
    }
    init_device();

    if (voltage >= 0.0) set_float_value(VOLTAGE_SET, voltage);
    if (current >= 0.0) set_float_value(CURRENT_SET, current);
    if (output == 1) enable_output();
    if (output == 0) disable_output();
    if (ovp >= 0) set_ovp(ovp);
    if (ocp >= 0) set_ocp(ocp);

    if (get_voltage)
	receive_response(0);
    if (get_current)
	receive_response(1);
    if (get_power)
	receive_response(2);
    if (get_info) {
	get_model_name();
        receive_response(3);
    }
    
    close_serial(disconnect);
    return 0;
}
