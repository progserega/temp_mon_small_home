#!/bin/bash
src="$1"
if [ -z "$src" ]
then
	echo "need 1 param: dump_spiffs file!"
	exit 1
fi
if [ ! -d files_from_dump ]
then
	mkdir files_from_dump
fi
/work/rtos/mkspiffs/mkspiffs -u files_from_dump $src
