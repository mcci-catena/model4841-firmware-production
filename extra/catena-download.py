#!/usr/bin/env python3

##############################################################################
#
# Module:  catena-download.py
#
# Function:
#   Push a file to Catena
#
# Copyright and License:
#   Copyright (C) 2021, MCCI Corporation. See accompanying LICENSE file
#
# Author:
#   Terry Moore, MCCI   April, 2021
#
##############################################################################

import argparse
import serial

global gVerbose
gVerbose = False

##############################################################################
#
# Name: ParseCommandArgs()
#
# Function:
#   Parse the command line arguments
#
# Definition:
#   ParseCommandArgs() -> Namespace
#
##############################################################################

def ParseCommandArgs():
    def checkPath(s):
        result = Path(s)
        if not result.is_dir():
            raise ValueError("not a directory")

        return result

    parser = argparse.ArgumentParser(description="download an image file to a Catena using the protocol demonstrated by catena-download.ino")
    parser.add_argument(
        "hInput",
        metavar="{inputBinFile}",
        type=argparse.FileType('rb'),
        help="Input binary file"
        )
    parser.add_argument(
        "sComPort", metavar="{portname}",
        help="Where to put generated output files"
        )
    parser.add_argument(
        "--baud", "-b",
        help="baud rate",
        default=115200
        )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="verbose output",
        default=False
        )
    parser.add_argument(
        "--command", "-c", metavar="{command}",
        help="Command to send to trigger update",
        default="system update",
        dest="sCommand"
        )
    return parser.parse_args()

# main function
def main():
    global gVerbose
    args = ParseCommandArgs()
    gVerbose = args.verbose

    # get the command as a buffer
    # read the file
    fileImage = args.hInput.read(-1)
    args.hInput.close()
    if gVerbose:
        print("Read file: size {}".format(len(fileImage)))
    v = memoryview(fileImage)

    # open the serial port
    hPort = serial.Serial(args.sComPort, baudrate=args.baud)
    if gVerbose:
        print("Using port {}".format(hPort.name))

    # read and discare chars
    hPort.read(hPort.in_waiting);

    # send the command
    hPort.write(args.sCommand.encode('ascii') + b"\n");

    i = 0
    chunkSize = 128
    zpad = b'\0' * chunkSize

    while True:
        c = hPort.read(1)
        if gVerbose:
            print(c.decode(encoding="ascii"), end='', sep='', flush=True)
        if c == b'?':
            msg = hPort.read_until()
            if gVerbose:
                print(msg.decode(encoding="ascii"), end='', sep='', flush=True)
            print("Failed!")
            exit(1);
        if c == b'<':
            chunk = fileImage[i: i + chunkSize]
            if len(chunk) != 0:
                i += len(chunk)
                chunk = (chunk + zpad)[0 : chunkSize]
                hPort.write(chunk)
        elif c == b'O':
            msg = hPort.read_until()
            if gVerbose:
                print(msg.decode(encoding="ascii"), end='', sep='', flush=True)
            print("Success!")
            exit(0)
                    
#### the standard trailer #####
if __name__ == '__main__':
    main()
