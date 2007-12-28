#!/bin/bash

for i in repo/*.tar.bz2 ; do
	echo "- $i"
	tar jxf $i -C unpacked
done
