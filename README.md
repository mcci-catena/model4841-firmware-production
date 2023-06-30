# Model 4841 Production for LoRaWAN&reg; Technology

[![GitHub release](https://img.shields.io/github/release/mcci-catena/model4841-production-lorawan.svg)](https://github.com/mcci-catena/model4841-production-lorawan/releases/latest) [![GitHub commits](https://img.shields.io/github/commits-since/mcci-catena/model4841-production-lorawan/latest.svg)](https://github.com/mcci-catena/model4841-production-lorawan/compare/v1.3.2...main)

This sketch is the production firmware for the MCCI&reg; [Model 4841 Air Quality Sensor](https://mcci.io/buy-model4841).

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

- [Introduction](#introduction)
- [Installing and Building](#installing-and-building)
- [Firmware Update via Serial](#firmware-update-via-serial)
    - [Steps to update a Firmware via Serial](#steps-to-update-a-firmware-via-serial)
- [Overview](#overview)
- [Activities](#activities)
- [Primary Data Acquisition](#primary-data-acquisition)
- [Uplink Data](#uplink-data)
	- [Port 1](#port-1)
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
- [Required libraries](#required-libraries)
- [Meta](#meta)
	- [Release Notes](#release-notes)
	- [Trademarks and copyright](#trademarks-and-copyright)
	- [License](#license)
	- [Support Open Source Hardware and Software](#support-open-source-hardware-and-software)

<!-- /TOC -->
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->

## Introduction

The Model 4841 combines an MCCI Catena&reg; 4630 LPWAN radio sensor board with a Plantower PMS7003 air quality sensor. The 4630 has an STM32L0 processor, a LoRa radio based on the Semtech SX1276, a Sensirion SHT3x temperature/humidity sensor, and a Sensirion SGPC3 gas sensor on board. This firmware runs on the STM32L0 processor in the 4630.

## Installing and Building

The best way to install and build this software is to start with [COLLECTION-model4841](https://github.com/mcci-catena/COLLECTION-model4841), a top-level collection that includes this repository as a submodule, along with the required libraries and a build procedure that uses the [`arduino-cli`](https://arduino.github.io/arduino-cli/). It is, of course, also possible to install the libraries individually and build with a variety of build procedures. See "[Required libraries](#required-libraries)" for details.

## Firmware Update via Serial

This is a feature to load the firmware via serial without getting the board into DFU mode.
Version 1.4.0 and later version of the sketch supports this feature.

### Steps to update a Firmware via Serial

- As the feature is available from version 1.4.0 and later of the sketch, the existing firmware on board should be the version v1.4.0 or later version firmware.
- Clone the repository in a desired location.
- Place the bin file of the sketch inside "extra" folder of the repository.
- Open the cmd prompt and navigate to "extra" folder where the bin file was placed.
- Run the following example command:

    ```py
    python catena-download.py -v -c "system update" model4841-production-lorawan.ino.bin COM10
    ```
- This will load the new firmware in Catena board with a "Success!" message.

    ```
    D:\IoT-Projects\Model4841\sketches\model4841-production-lorawan\extra>python catena-download.py -v -c "system update" model4841-production-lorawan.ino.bin COM10
    Read file: size 104108
    Using port COM10
    system update
    Update firmware: echo off, timeout 2 seconds
    <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<OK<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    Success!
    ```
- Once the firmware is loaded, RESET Model 4841, this will start to run the newly loaded sketch in Model 4841.

**Note**
- COM port (COM10 used in example command) might vary for different devices. Use the COM port which is enumerated for the Catena board in use.
- To find the COM port, open the "device manager" from start menu and find the COM port which is enumerated for the Catena board in use.
- Make sure the COM port is not connected to any Serial monitors.

## Overview

The primary function of the Catena 4630 is to capture and transmit real-time air quality and dust particle data to remote data consumers, using LoRaAWAN. For consistency with MCCI's other monitoring products, information is captured and transmitted at six minute intervals.

This firmware has the following features.

- During startup, the sketch initializes the PMS7003.

- If the device is provisioned as a LoRaWAN device, it enters the measurement loop.

- Every measurement cycle, the sketch powers up the PMS7003. It then takes a sequence of 10 measurements. Data is gathered from the atmospheric PM serias and the dust series. For each series, outliers are discarded using an IQR1.5 filter, and then the remaining data is averaged.

- Temperature and humidity are measured by the SHT3x.

- Air quality value (Total Volatile Organic Compounds, or "TVOC") read from the SGPC3 sensor.

- Data is encoded into binary and transmitted to the LoRaWAN network, using LoRaWAN port 1, as a class A device.

- The sketch uses [mcci-catena/SGPC3](https://github.com/mcci-catena/SGPC3) library to measure TVOC value and transmit over network. This library is based on the [gb88/SGPC3](https://github.com/gb88/SGPC3) library, with light modifications for LoRaWAN compatibility.

- The sketch uses the [Catena Arduino Platform](https://github.com/mcci-catena/Catena-Arduino-Platform.git), and therefore the basic provisioning commands from the platform are always available while the sketch is running. This also allows user commands to be added if desired.

- The `McciCatena::cPollableObject` paradigm is used to simplify the coordination of the activities described above.

## Activities

1. [Setup](#setup): an activity that launches the other activities.
2. [Primary Data Acquisition](#primary-data-acquisition): the activity that configures the PMS7003, and then takes data. It wakes up periodically, poll registers, computes and send an uplink on LoRaWAN port 5.
3. [LoRaWAN control](#lorawan-control): this activity manages transmission and reception of data over LoRaWAN.
4. [Local serial command processor](#commands)
5. *[Future]* A firmware update activity
6. *[Future]* An activity to handle copying firmware

## Primary Data Acquisition

The primary loop wakes up based on elapsed time, makes a series of measurements, scales them, and transmits them.

It senses temp sensor data, TVOC data, particulat matter data, dust particle concentrations, battery level, and boot count. By default, the cycle time is every six minutes, but this can be adjusted using the cycle-time downlink.

## Uplink Data

### Port 1

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

Note that the libraries mentioned here are actually govenered by the versions chosen in [COLLECTION-model4841](https://github.com/mcci-catena/COLLECTION-model4841).

v1.4.0 has the following changes.

- Updated the production sketch with a new feature "Firmware Update via Serial" to avoid getting the board into DFU mode.
- Use the following library:

   | Name | Version | Comments
   |------|:-------:|:-----------
   | BSP  | 3.1.0   | Pick up latest release
   | Catena-Arduino-Platform | 0.21.2 | Pick up bug fixes

v1.3.2 only has changes in the build system.

   | Name | Version | Comments
   |------|:-------:|:-----------
   | tools-build-with-cli  | 2.0.1   | set build artifact prefix independtly of sketch name.

v1.3.1 has the following changes. All are patch changes in libraries, so there's only a patch-release bump.

- Update the following libraries.

   | Name | Version | Comments
   |------|:-------:|:-----------
   | BSP  | 3.0.5   | Pick up error fix for USB hot interrupt problem
   | arduino-lmic | 4.2.0-pre1 | Pick up network time support.
   | MCCI-Catena-PMS7003 | 0.1.2 | Pick up official relase (no code changes)
   | MCCI-Catena-SHT3x | 0.2.1 | pick up official release (no code changes)

v1.3.0 has the following changes

- Updated libraries, thereby adding the `lorawan status` and `lorawan subband` commands, thereby causing a minor version bump.
- Updated COLLECTION to use common build system.

v1.2.0 has the following changes

- Fix [#1](https://github.com/mcci-catena/model4841-production-lorawan/issues/1): initialize radio before initializing sensors, so that commands are present.

- Using the following libraries:

   | Name | Version | Comments
   |------|:-------:|:-----------
   |Catena-Arduino-Platform | 0.20.0.30 | Pick up error fix for 59-day hang
   |Arduino-LoRaWAN | 0.8.0 |
   |Arduino-LMIC | 3.3.0 |
   |MCCIADK | 0.2.2 |
   |MCCI-Arduino-BSP | 2.8.0 |

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

This document and the contents of this repository are copyright 2019-2022, MCCI Corporation.

### License

This repository is released under the [MIT license](./LICENSE.md). Commercial licenses are also available from MCCI Corporation.

### Support Open Source Hardware and Software

MCCI invests time and resources providing this open source code, please support MCCI and open-source hardware by purchasing products from MCCI, Adafruit and other open-source hardware/software vendors!

For information about MCCI's products, please visit [store.mcci.com](https://store.mcci.com/).
