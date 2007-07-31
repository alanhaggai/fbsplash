#!/bin/bash

cnt=25
sum=0

for i in `seq 0 $cnt`
do
	res=`/usr/bin/time -f %U ./benchmark 2>&1`
	sum=`echo "$sum + $res" | bc`
done

echo "scale=2 ; $sum / $cnt" | bc



