# Model 4841 Production for LoRaWAN&reg; Technology

This sketch is the production firmware for the MCCI&reg; Model 4841 Air Quality Sensor.

**Contents:**
<!--
  This TOC uses the VS Code markdown TOC extension AlanWalk.markdown-toc.
  We strongly recommend updating using VS Code, the markdown-toc extension and the
  bierner.markdown-preview-github-styles extension.  Note that if you are using
  VS Code 1.29 and Markdown TOC 1.5.6, https://github.com/AlanWalk/markdown-toc/issues/65
  applies -- you must change your line-ending to some non-auto value in Settings>
  Text Editor>Files.  `\n` works for me.
-->

<!-- markdownlint-disable MD033 MD004 -->
<!-- markdownlint-disable -->
<!-- TOC depthFrom:2 updateOnSave:true -->

- [System configuration](#system-configuration)
	- [Sample interval](#sample-interval)
- [Activities](#activities)
- [Primary Data Acquisition](#primary-data-acquisition)
- [Uplink Data](#uplink-data)
	- [port 1](#port-1)
	- [Bitmap fields and associated fields](#bitmap-fields-and-associated-fields)
	- [Battery Voltage (field 0)](#battery-voltage-field-0)
	- [System Voltage (field 1)](#system-voltage-field-1)
	- [Bus Voltage (field 2)](#bus-voltage-field-2)
	- [Boot counter (field 3)](#boot-counter-field-3)
	- [Environmental readings (field 4)](#environmental-readings-field-4)
	- [Particulate Matter (field 5)](#particulate-matter-field-5)
	- [Dust (field 6)](#dust-field-6)
	- [TVOC (field 7)](#tvoc-field-7)
- [Data Formats](#data-formats)
	- [uint16](#uint16)
	- [int16](#int16)
	- [uint8](#uint8)
	- [int32](#int32)
	- [float32](#float32)
- [Commands](#commands)
	- [`debugmask`](#debugmask)
	- [`run`, `stop`](#run-stop)
	- [`stats`](#stats)
	- [`wake`](#wake)
- [Required libraries](#required-libraries)
- [Meta](#meta)
	- [Release Notes](#release-notes)
	- [Trademarks and copyright](#trademarks-and-copyright)
	- [License](#license)
	- [Support Open Source Hardware and Software](#support-open-source-hardware-and-software)

<!-- /TOC -->
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->

## System configuration

The Model 4841 combines an MCCI Catena&reg; 4630 LPWAN radio sensor board with a Plantower PMS7003 air quality sensor. This firmware runs on the STM32L0 processor in the 4630.

### Sample interval

The primary function of the Catena 4630 is to capture and transmit real-time air quality and dust particle data to remote data consumers, using LoRaAWAN. For consistency with MCCI's other monitoring products, information is captured and transmitted at six minute intervals.

This firmware has the following features.

- During startup, the sketch initializes the PMS7003.

- If the device is provisioned as a LoRaWAN device, it enters the measurement loop.

- Every measurement cycle, the sketch powers up the PMS7003. It then takes a sequence of 10 measurements. Data is gathered from the atmospheric PM serias and the dust series. For each series, outliers are discarded using an IQR1.5 filter, and then the remaining data is averaged.

- Current environmental conditions are read from the SHT3x.

- Current air quality value (TVOC) read from the SGPC3 sensor.

- Data is prepared using port 5 format 0x21, and transmitted to the network.

- The sketch uses [mcci-catena/SGPC3](https://github.com/mcci-catena/SGPC3) library to measure TVOC value and transmit over network. This library is based on the [gb88/SGPC3](https://github.com/gb88/SGPC3) library, with light modifications for LoRaWAN compatibility.

- The sketch uses the [Catena Arduino Platform](https://github.com/mcci-catena/Catena-Arduino-Platform.git), and therefore the basic provisioning commands from the platform are always available while the sketch is running. This also allows user commands to be added if desired.

- The `McciCatena::cPollableObject` paradigm is used to simplify the coordination of the activities described above.

## Activities

1. [Setup](#setup): an activity that launches the other activities.
2. [Primary Data Acquisition](#primary-data-acquisition): the activity that configures the PMS7003, and then takes data. It wakes up periodically, poll registers, computes and send an uplink on LoRaWAN port 1.
3. [LoRaWAN control](#lorawan-control): this activity manages transmission and reception of data over LoRaWAN.
4. [Local serial command processor](#commands)
5. *[Future]* A firmware update activity
6. *[Future]* An activity to handle copying firmware

## Primary Data Acquisition

The primary loop wakes up, read a few predefined registers, scales them, and transmits them in a form similar to sensor1 app.

It can transmit up to six totals. We will sense temp sensor data, Air quality, dust particle and boot count. We will honor the cycle-time command.

Thus, if we sample demand every six minutes, we'll get the value recorded for the most recent six minutes. We may (and in general, we will) have sampling error and drift, but the averaging will be roughly equivalent.

## Uplink Data

### port 1

Port 1 is used for normal uplink demand data. It is the usual bitmap/data formant.

It has the usual scheme:

byte 0: id (0x21)

byte 1: flag byte

bytes 2..n: variable data, governed by flag byte.

The flags:

bit 0: `Vbat` is present.

We transmit:

bytes | format   | units      | contents
------|:--------:|:----------:|---------
0     | [`uint8`](#uint8)  | number     | `0x21`, identifying the data format.
1     | [`uint8`](#uint8) | flag byte  | bitmask describing data fields that follow.
2..n  | -        |  -         | data bytes; use bitmap to decode.

### Bitmap fields and associated fields

The bitmap byte has the following interpretation.

Bit | Field length (bytes) | Data format |Description
:---:|:---:|:---:|:----
0 | 2 | [`int16`](#int16) | [Battery voltage](#battery-voltage-field-0)
1 | 2 | [`int16`](#int16) | [System voltage](#sys-voltage-field-1)
2 | 2 | [`int16`](#int16) | [Bus voltage](#bus-voltage-field-2)
3 | 1 | [`uint8`](#uint8) | [Boot counter](#boot-counter-field-3)
4 | 4 | [`int16`](#int16), [`uint16`](#uint16) | [uint16](#uint16) | [Temperature, humidity](environmental-readings-field-4)
5 | 2 | 3 x [`uint16`](#uint16) | [Particulate matter](#particulate-matter-field-5)
6 | 2 | 6 x [`uint16`](#uint16) | [Dust](#dust-field-6)
7 | 1 | [`uint16`](#uint16) | [TVOC](#tvoc-field-7)

### Battery Voltage (field 0)

Field 0, if present, carries the current battery voltage. To get the voltage, extract the int16 value, and divide by 4096.0. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

### System Voltage (field 1)

Field 1, if present, carries the current System voltage. Divide by 4096.0 to convert from counts to volts. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

Note: this field is not transmitted by the firmware.

### Bus Voltage (field 2)

Field 2, if present, carries the current voltage from USB VBus. Divide by 4096.0 to convert from counts to volts. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

### Boot counter (field 3)

Field 3, if present, is a counter of number of recorded system reboots, modulo 256.

### Environmental readings (field 4)

Field 4, if present, has two environmental readings as four bytes of data.

- The first two bytes are a int16 representing the temperature (divide by 256 to get degrees Celsius).

- The next two bytes are a uint16 representing the relative humidity (divide by 65535 to get percent). This field can represent humidity from 0% to 100%, in steps of roughly 0.001529%.

### Particulate Matter (field 5)

Field 5, if present, represents the particulate matter (PM1.0, PM2.5, PM10) in the atmosphere.

### Dust (field 6)

Field 6, is present, represents the Dust (PM0.3, PM0.5, PM1.0, PM2.5, PM5, PM10) in the environment.

### TVOC (field 7)

Field 7, is present, represents the air quality TVOC in the environment.

## Data Formats

All multi-byte data is transmitted with the most significant byte first (big-endian format).  Comments on the individual formats follow.

### uint16

an integer from 0 to 65536.

### int16

a signed integer from -32,768 to 32,767, in two's complement form. (Thus 0..0x7FFF represent 0 to 32,767; 0x8000 to 0xFFFF represent -32,768 to -1.)

### uint8

an integer from 0 to 255.

### int32

a signed integer from -2,147,483,648 to 2,147,483,647, in two's complement form. (0x00000000 through 0x7FFFFFFF represent 0 to 2,147,483,647; 0x80000000 to 0xFFFFFFFF represent -2,147,483,648 to -1.)

### float32

An IEEE-754 single-precision floating point number, in big-endian byte order. The layout is:

  31 | 30..23 | 22..0
:---:|:------:|:-----:
sign | exponent | mantissa

The sign bit is set if the number is negative, reset otherwise. Note that negative zero is possible.

The exponent is the binary exponent to be applied to the mantissa, less 127. (Thus an exponent of zero is represented by 0x7F in this field).

The mantissa is the least significant 23 bits of the binary fraction, starting at bit -1 (the "two-to-the-1/2" bit). The effective mantissa is the represented mantissa, plus 1 if the exponent field is not 0x7F.

The following JavaScript code extracts a `float32` starting at position `i` from buffer `bytes`. It handles NANs and +/- infinity, which are all represented with `uExp` == 0xFF. JavaScript doesn't distinguish quiet and signaling NaNs, nor does it have signed NaNs. So for all NaNs, the result is just `Number.NaN`.

```javascript
function u4toFloat32(bytes, i) {
    // pick up four bytes at index i into variable u32
    var u32 = (bytes[i] << 24) + (bytes[i+1] << 16) + (bytes[i+2] << 8) + bytes[i+3];

    // extract sign, exponent, mantissa
    var bSign =     (u32 & 0x80000000) ? true : false;
    var uExp =      (u32 & 0x7F800000) >> 23;
    var uMantissa = (u32 & 0x007FFFFF);

    // if non-numeric, return appropriate result.
    if (uExp == 0xFF) {
      if (uMantissa == 0)
        return bSign ? Number.NEGATIVE_INFINITY
                     : Number.POSITIVE_INFINITY;
        else
            return Number.NaN;
    // else unless denormal, set the 1.0 bit
    } else if (uExp != 0) {
      uMantissa +=   0x00800000;
    } else { // denormal: exponent is the minimum
      uExp = 1;
    }

    // make a floating mantissa in [0,2); usually [1,2), but
    // sometimes (0,1) for denormals, and exactly zero for zero.
    var mantissa = uMantissa / 0x00800000;

    // apply the exponent.
    mantissa = Math.pow(2, uExp - 127) * mantissa;

    // apply sign and return result.
    return bSign ? -mantissa : mantissa;
}
```

## Commands

In addition to the [default commands](https://github.com/mcci-catena/Catena-Arduino-Platform#command-summary) provided by the library, the sketch provides the following commands:

### `debugmask`

Get or set the debug mask, which controls the verbosity of debug output from the library.

To get the debug mask, enter command `debugmask` on a line by itself.

To set the debug mask, enter <code>debugmask <em><u>number</u></em></code>, where *number* is a C-style number indicating the value. For example, `debugmask 0x31` is the same as `debugmask 49` -- it turns on bits 0, 4, and 5.

The following bits are defined.

Bit  |   Mask     |  Name        | Description
:---:|:----------:|--------------|------------
  0  | 0x00000001 | `kError`     | Enables error messages
  1  | 0x00000002 | `kWarning`   | Enables warning messages (none are defined at present)
  2  | 0x00000004 | `kTrace`     | Enables trace messages. This specifically causes the FSM transitions to be displayed.
  3  | 0x00000008 | `kInfo`      | Enables informational messages (none are defined at present)
  4  | 0x00000010 | `kTxData`    | Enable display of data sent by the library to the PMS7003
  5  | 0x00000020 | `kRxDiscard` | Enable display of discarded receive data bytes

### `run`, `stop`

Start or stop the measurement loop. After boot, the measurement loop is enabled by default.

### `stats`

Display the receive statistics. The library keeps track of spurious characters and messages; this is an easy way to get access.

### `wake`

Bring up the PMS7003. This event is abstract -- it requests the library to do whatever's needed (powering up the PMS7003, waking it up, etc.) to get the PMS7003 to normal state.


## Required libraries

The following Arduino standard libraries are used in this project.

- SPI
- Wire

The following additional libraries are used in this project.

- [Arduino LMIC](https://github.com/mcci-catena/arduino-lmic/)
- [Arduino LoRaWAN](https://github.com/mcci-catena/arduino-lorawan)
- [Catena Arduino Platform](https://github.com/mcci-catena/Catena-Arduino-Platform)
- [Catena MCCIADK](https://github.com/mcci-catena/Catena-mcciadk)
- [MCCI-CatenaPMS7003](https://github.com/mcci-catena/MCCI-Catena-PMS7003)
- [MCCI FRAM I2C](https://github.com/mcci-catena/MCCI_FRAM_I2C)
- [SGPC3](https://github.com/mcci-catena/SGPC3)
- [MCCI-Catena-SHT3x](https://github.com/mcci-catena/MCCI-Catena-SHT3x)

## Meta

### Release Notes

v1.1.0 has the following changes.

- Use the following libraries:

   | Name | Version |
   |------|:-------:|
   |Catena-Arduino-Platform | 0.20.0 |
   |Arduino-LoRaWAN | 0.8.0 |
   |Arduino-LMIC | 3.3.0 |
   |MCCIADK | 0.2.2 |
   |MCCI-Arduino-BSP | 2.8.0 |

### Trademarks and copyright

MCCI and MCCI Catena are registered trademarks of MCCI Corporation. LoRa is a registered trademark of Semtech Corporation. LoRaWAN is a registered trademark of the LoRa Alliance.

This document and the contents of this repository are copyright 2019-2021, MCCI Corporation.

### License

This repository is released under the [MIT](./LICENSE) license. Commercial licenses are also available from MCCI Corporation.

### Support Open Source Hardware and Software

MCCI invests time and resources providing this open source code, please support MCCI and open-source hardware by purchasing products from MCCI, Adafruit and other open-source hardware/software vendors!

For information about MCCI's products, please visit [store.mcci.com](https://store.mcci.com/).
