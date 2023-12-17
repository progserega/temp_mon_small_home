#!/bin/bash
project_name="${PWD##*/}"
#echo $project_name
file_store="spiffs_0x110000_0xF0000_${project_name}_$(date +%Y.%m.%d).bin"

echo "dump flash from address 0x110000 size 0xF0000 to file: $file_store..."
esptool.py --port /dev/ttyUSB0 -b 115200 read_flash 0x110000 0xF0000 $file_store
