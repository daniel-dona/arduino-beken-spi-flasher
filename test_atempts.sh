#!/bin/bash
for i in {1..25}
do
  python3 flash_tool.py --port /dev/ttyUSB1 --baud 1000000 read --size 0x200000 0 test_$i.bin
  md5sum test_$i.bin
done