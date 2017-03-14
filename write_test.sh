#!/bin/bash

## USAGE 
## bash write_test.sh <number of iterations> [block_size]
## Default is 16 threads, 5 iterations

k=$1

if [ -z $k ]; then
   k=5
fi

b=$2

if [ -z $b ]; then
   b=4096
fi



for t in {1,2,4,6,8,12,16}; do
    for i in $(seq 1 $k); do
	./file_wr.exe tc$t-size2GB-block$b -tc $t -max $t -b $b
    done;
done
