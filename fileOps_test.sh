#!/bin/bash

k=5
n=5000
for t in {1,2,4,6,8,12,16}; do
	for i in $(seq 1 $k); do 
	     rm -rf /mnt/ramdisk/*
	     for j in $(seq 1 16); do
		mkdir /mnt/ramdisk/dir$j
	     done;
	     ./file_ops.exe tc$t-n$n-create -tc $t -max $t -n $n
	     #ls /mnt/ramdisk/*
	     ./file_ops.exe tc$t-n$n-rename -tc $t -max $t -n $n -o 2
	     #ls /mnt/ramdisk/*
        done;
done;
