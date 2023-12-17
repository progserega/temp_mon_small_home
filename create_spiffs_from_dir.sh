#!/bin/bash
src="$1"
if [ -z "$src" ]
then
	echo "need 1 param: dir with files!"
	exit 1
fi
out_file="spiffs_dump_fs_new.bin"
echo "create spiffs dump $out_file from dir $src"
/work/rtos/mkspiffs/mkspiffs -c $src -b 4096 -p 256 -s 0xF0000 $out_file
