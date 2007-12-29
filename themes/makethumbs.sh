#!/bin/bash

orig=1024x768
new=300x225

for i in shots/${orig}*.png ; do
	echo "$i"
	f=${i//$orig/thumbs\/$new}
	convert -thumbnail ${new} $i ${f//png/jpg}
done
