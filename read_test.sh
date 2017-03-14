#!/bin/bash

## USAGE 
## bash read_test.sh <number of iterations> [random_read]
## Default is 5 iterations and sequential read.
## It runs for 16 threads.

k=$1

if [ -z $k ]; then
   k=5
fi

r=$2

if [ -z $r ]; then
   r=0
fi

for t in {1,2,4,6,8,12,16}; do
    for i in $(seq 1 $k); do
	if [ $r = 0 ]; then
	    ./file_rd.exe tc$t-size2GB-block4KB-sequential -tc $t -max $t
	else
	    ./file_rd.exe tc$t-size2GB-block4KB-random -tc $t -max $t -r 1	
	fi
    done;
done
