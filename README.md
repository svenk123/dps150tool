# DPS150 Control Tool – `dps150tool`

## 1. Purpose

`dps150tool` is a minimalistic command-line utility designed to control and monitor the **FNIRSI DPS150PD** programmable DC power supply. It is intentionally kept very simple:

> Each execution of the tool sends exactly **one command** to the power supply.

This makes it ideal for usage in **custom shell scripts**, automated testing, or remote control setups where reliability and predictability are key.

It allows users to:
- Set voltage and current
- Enable or disable output
- Configure protection settings
- Query output state or device information

## 2. Supported Device

This tool supports the **FNIRSI DPS150PD**, a compact, fully programmable DC bench power supply from the Chinese manufacturer **FNIRSI**.

Key features of the DPS150PD include:
- Output up to **30V / 5A** with a resolution of **10mV / 1mA**
- Total output power up to **150W**, depending on the power input
- **Compact and portable** form factor, ideal for small workbenches or mobile setups
- USB-C interface for digital control and firmware updates
- Advanced features like:
  - **Sequential output**
  - **Voltage/current sweep and scan**
  - **Real-time monitoring**
  - **Curve recording**
  - **Preset management** with 6 user-defined profiles for fast switching
- IPS display rotatable by 90°, allowing comfortable viewing angles

**Note:**  
The maximum achievable output power depends on the connected input source.  
The included power adapter delivers **20V / 5A (100W)**.  
For full 150W performance, an external power supply with **30V via USB-PD or 5.5×2.5mm DC plug** is required.

## 3. Supported Platforms

This software currently supports **Linux** systems. It is written in portable C and uses POSIX APIs for serial communication. Other Unix-like systems may work, but are untested.

## 4. Installation

To install the tool, follow these steps:

```bash
git clone https://github.com/svenk123/dps150tool.git
cd dps150tool
make
```

The compiled binary will be named `dps150tool`.

## 5. Connecting the Power Supply

To connect the DPS150 to your PC:

1. Use a **USB-A to USB-C or micro USB** cable (depending on the DPS150 model).
2. Once plugged in, the system should detect a USB serial device, usually at `/dev/ttyUSB0` or `/dev/ttyACM0`.
3. Ensure your user has permission to access the serial device (e.g. add yourself to the `dialout` group):

```bash
sudo usermod -aG dialout $USER
```

Then log out and back in.

## 6. Command-Line Options

| Option | Description |
|--------|-------------|
| `-d <device>`     | Specify the serial device (default: `/dev/ttyUSB0`) |
| `-u <voltage>`    | Set output voltage (0.0 to 30.0 V) |
| `-i <current>`    | Set output current (0.0 to 5.0 A) |
| `-x 0,1`          | Disable or enable Over Voltage Protection (OVP) |
| `-y 0,1`          | Disable or enable Over Current Protection (OCP) |
| `-o 0,1`          | Turn output OFF (0) or ON (1) |
| `-U`              | Read and display current output voltage |
| `-I`              | Read and display current output current |
| `-V`              | Read and display device information (model, firmware, hardware) |
| `-z`              | Do not send disconnect command before closing the port |

## 7. Example Usage

Set voltage to 12V and enable output:
```bash
./dps150tool -u 12.0 -o 1
```

Set voltage and current, enable output, and turn on OVP:
```bash
./dps150tool -u 5.0 -i 1.0 -x 1 -o 1
```

Query output voltage and current:
```bash
./dps150tool -U -I
```

Display device information:
```bash
./dps150tool -V
```

Use alternative device path and skip disconnect command:
```bash
./dps150tool -d /dev/ttyUSB1 -z
