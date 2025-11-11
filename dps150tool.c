/*****************************************************************************
 *
 * Copyright (c) 2025 Sven Kreiensen
 * All rights reserved.
 *
 * You can use this software under the terms of the MIT license
 * (see LICENSE.md).
 *
 * THE SOFTWARE IS PROVIDED .AS IS., WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define HEADER_INPUT 0xF0
#define HEADER_OUTPUT 0xF1
#define CMD_GET 0xA1
#define CMD_SET 0xB1
#define CMD_XXX_176 0xB0
#define CMD_XXX_193 0xC1

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

#define SOFTWARE_VERSION "1.0"

int serial_fd;
char *device = "/dev/ttyUSB0";
int debug; // Debug flag

/**
 * Opens the serial port
 * @param device The device to open
 * @return 0 on success, -1 on error
 */
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

/**
 * Send command
 * @param c1 The first command byte
 * @param c2 The second command byte
 * @param c3 The third command byte
 * @param c4 The fourth command byte
 * @param c5 The fifth command byte
 * @return 0 on success, -1 on error
 */
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
  (void)write(serial_fd, command, sizeof(command));
  usleep(50000);
}

/**
 * Print printable characters from buffer
 * @param label The label to print
 * @param data The data to print
 * @param length The length of the data
 */
void print_printable_string(const char *label, uint8_t *data, int length) {
  printf("%s", label);

  for (int i = 0; i < length; i++) {
    if (data[i] >= 0x20 && data[i] <= 0x7E) {
      printf("%c", data[i]);
    }
  }
  
  printf("\n");
}

/**
 * Parse response
 * @param response_type The type of response to parse
 */
void receive_response(int response_type) {
  uint8_t buffer[1024];

  int bytes_read = read(serial_fd, buffer, sizeof(buffer));
  if (bytes_read > 0) {
    /* Print raw data for debugging */
    if (debug) {
      printf("Received: ");
      for (int i = 0; i < bytes_read; i++) {
        if (buffer[i] >= 0x20 && buffer[i] <= 0x7E) {
          printf("%02X (%03d) '%c'\n", buffer[i], buffer[i], buffer[i]);
        } else {
          printf("%02X (%03d) '?'\n", buffer[i], buffer[i]);
        }
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
          memcpy(&value, &buffer[4 + 4], sizeof(float));
          printf("Output Current: %.3fA\n", value);
        }
        if (response_type == 2) {
          memcpy(&value, &buffer[4 + 8], sizeof(float));
          printf("Output Power: %.2fW\n", value);
        }
        break;
      case 222:
        print_printable_string("Device Model: ", &buffer[4], buffer[3]);
        break;
      case 223:
        print_printable_string("Hardware Version: ", &buffer[4], buffer[3]);
        break;
      case 224:
        print_printable_string("Firmware Version: ", &buffer[4], buffer[3]);
        break;
      default:
        printf("Unknown response\n");
      }
    }
  }
}

/**
 * Send float value i.e. voltages, currents,...
 * @param type The type of value to send
 * @param value The value to send
 */
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

/**
 * Enable output
 */
void enable_output() { 
    set_byte_value(OUTPUT_ENABLE, 1); 
}

/**
 * Disable output
 */
void disable_output() { 
    set_byte_value(OUTPUT_ENABLE, 0); 
}

/**
 * Enable or disable over-voltage protection
 * @param state The state to set
 */
void set_ovp(uint8_t state) { 
    set_byte_value(OVP, state); 
}

/**
 * Enable or disable over-current protection
 * @param state The state to set
 */
void set_ocp(uint8_t state) { 
    set_byte_value(OCP, state); 
}

/**
 * Get model name
 */
void get_model_name() {
  send_command(HEADER_OUTPUT, CMD_GET, MODEL_NAME, 0, 0);
}

/**
 * Initialize the power supply communition
 */
void init_device() {
  uint8_t baudrate_index = 4; // 115200 baud

  send_command(HEADER_OUTPUT, CMD_XXX_193, 0, (uint8_t[]){1}, 1);
  send_command(HEADER_OUTPUT, CMD_XXX_176, 0, &baudrate_index, 1);
}

/**
 * Close the serial port
 * @param disconnect Whether to disconnect the device
 */
void close_serial(int disconnect) {
  if (disconnect) {
    send_command(HEADER_OUTPUT, CMD_XXX_193, 0, NULL, 0);
  }

  close(serial_fd);
}

/**
 * Print usage
 */
void usage(const char *program_name) {
  fprintf(stderr,
          "Usage: %s [-d device] [-u voltage] [-i current] [-x 0|1] [-y "
          "0|1] [-U] [-I] [-P] [-V] [-o 0|1] [-z] [-v]\n"
          "Version: %s\n",
          program_name, SOFTWARE_VERSION);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  float voltage = -1.0, current = -1.0;
  int output = -1, ovp = -1, ocp = -1, get_voltage = 0, get_current = 0,
      get_power = 0, get_info = 0;
  int opt;
  int disconnect = 1;
  debug = 0;

  if (argc == 1) {
    usage(argv[0]);
  }

  while ((opt = getopt(argc, argv, "d:u:i:x:y:UIPVo:zv")) != -1) {
    switch (opt) {
    case 'd':
      device = optarg;
      break;
    case 'u':
      voltage = atof(optarg);
      break;
    case 'i':
      current = atof(optarg);
      break;
    case 'x':
      ovp = atoi(optarg);
      break;
    case 'y':
      ocp = atoi(optarg);
      break;
    case 'U':
      get_voltage = 1;
      break;
    case 'I':
      get_current = 1;
      break;
    case 'P':
      get_power = 1;
      break;
    case 'V':
      get_info = 1;
      break;
    case 'o':
      output = atoi(optarg);
      break;
    case 'z':
      disconnect = 0;
      break;
    case 'v':
      debug = 1;
      break;
    default:
      usage(argv[0]);
    }
  }

  if (open_serial(device) != 0) {
    return 1;
  }

  init_device();

  if (voltage >= 0.0)
    set_float_value(VOLTAGE_SET, voltage);
  if (current >= 0.0)
    set_float_value(CURRENT_SET, current);
  if (output == 1)
    enable_output();
  if (output == 0)
    disable_output();
  if (ovp >= 0)
    set_ovp(ovp);
  if (ocp >= 0)
    set_ocp(ocp);

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
