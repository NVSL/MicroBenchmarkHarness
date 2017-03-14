#!/bin/bash

k=5
n=5000
for t in {1,2,4,6,8,12,16}; do
	for i in $(seq 1 $k); do 
	     rm -rf /mnt/ramdisk/*
	     ./file_ops.exe tc$t-n$n-create -tc $t -max $t -n $n
	     ./file_ops.exe tc$t-n$n-rename -tc $t -max $t -n $n -o 2
        done;
done;
