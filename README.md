# light_display
Embedded System for 8-Channel WS2812b Light Display.

The system is implemented on an Altera DE0 development board.

Firmware running on the Cyclone III chip includes:
   - A lightweight SD card reader
   - 8 WS2812b transmitter channels
   - 8 1024KB FIFO buffers for color data

Software running on the NIOS II Embedded processor includes:
   - SD card initialization
   - Lightweight FAT32 driver
   - Processing for custom *.ldf color data file format

## History
This personal project was started in 2018 and first unveiled in 2020 as part of my father's Christmas Light display

[Stuffington, CA](https://www.facebook.com/StuffingtonChristmas/).

## Note
This repository contains only selected source files written in the C language and Verilog HDL, as well as select documentation. It is not intended to be a complete usable package.
