#!/bin/bash
src="$1"
if [ -z "$src" ]
then
	echo "need 1 param: spiffs dump file to upload!"
	exit 1
fi

echo "upload flash dump $src to esp32 at address 0x110000 size 0xF0000..."
esptool.py --port /dev/ttyUSB0 -b 115200 --chip esp32 write_flash -z 0x110000 $src
