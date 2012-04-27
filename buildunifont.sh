#!/bin/bash

OUTFILE=unifont.h
INFILE=unifont.hex

# append a line
function a {
	echo "$1" >> "$OUTFILE"
}

rm -f "$OUTFILE" 
a "#ifndef _UNIFONT_H"
a "#define _UNIFONT_H"
a "static char *unifont[] = {"

count=0
for line in `cat "$INFILE"`; do
	index=$(echo "ibase=16;$(echo "$line"|sed -e 's/:.*$//g')"|bc)
	if [ $count -eq $index ]; then
		a "\"$(echo "$line"|sed -e 's/^.*://g')\","
	else
		until [ $count -eq $index ]; do
			let count=count+1;
			a "\"\","
		done
		a "\"$(echo "$line"|sed -e 's/^.*://g')\","
	fi
	let count=count+1;
done
a "};"
a "#endif"

