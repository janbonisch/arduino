#!/bin/bash
avrdude -v -patmega328p -Uflash:w:grbl/grbl.hex:i -carduino -b 57600 -P /dev/ttyUSB0
