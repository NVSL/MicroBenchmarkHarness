#!/bin/bash

HEAP_PATH="/mnt/ram/heap"
RUN_TIME=30 # Sec
FOOT=4096 # MB

for TC in 1 2 4 8 16 32 64; do
    for M in 'rnd' 'seq'; do
        for G in 64 128 256 512 1024 2048 4096 8192; do
            rm -rf $HEAP_PATH
            ./dax_load.exe $TC-$M-$G -tc $TC -footMB $FOOT -rt $RUN_TIME -file $HEAP_PATH -m $M -g $G
        done
    done
done
