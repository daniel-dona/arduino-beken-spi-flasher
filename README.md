# arduino-beken-spi-flasher

I just lost patience trying to make spidev work on Armbian with the latest kernel, so I wrote a SPI flasher for Arduino based on existing scripts for setting BK72XX into SPI flash mode.

I just needed this to reflash a working bootloader on Beken based A9 cameras, is possible other things are broken or will not work. Use at your own risk!

## Connections
```
Arduino     <------>     BK72XX
13 (SCK )   <------>     CLK/TCLK
12 (MISO)   <------>     SO/MISO
11 (MOSI)   <------>     SI/MOSI
10 (CS  )   <------>     CS/TMS
```
Naming could change depending on PCB silkscreen

## Usage

1. Flash the Arduino board with the .ino project, use Arduino IDE or Arduino CLI.
2. Install Python libs:
```
python3 -m venv .venv
source .venv/bin/activate
pip3 install -r requirements.txt
```
3. Reading:
```
python3 flash_tool.py --port /dev/ttyUSB0 --baud 1000000 read 0 dump.bin
```
4. Writing
```
python3 flash_tool.py --port /dev/ttyUSB0 --baud 1000000 write 0 dump.bin
```
5. 
## Limitations

- Configured to work only with XTX flash chips on BK7252 QFN48 chips, change SPI ID for others.
- Not all Arduino boards support 1M baud rate, if you find connection problems try other baud rates. Change it on project and when calling the Python script.
```
  //Serial.begin(115200);
  //Serial.begin(500000);
  Serial.begin(1000000);
```
- Are really 250 `D2` bytes required?

