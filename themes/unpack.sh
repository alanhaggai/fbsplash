#!/bin/bash

for i in repo/*.tar.bz2 ; do
	echo "- $i"
	tar jxf "$i" -C unpacked
	dir=$(tar jtf "$i" | head -n1)
	echo "$i" > "unpacked/$dir/.origin"
done
