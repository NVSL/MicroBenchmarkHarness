#!/bin/bash

HEAP_PATH="/mnt/pmem0/heap"
RUN_TIME=30 # Sec
FOOT=40960 # 40 GB

for TC in 1 4 32; do
    for M in 'rnd' 'seq'; do
        for G in 64 1024; do
            rm -rf $HEAP_PATH
            ./dax_load.exe $M/$G -tc $TC -footMB $FOOT -rt $RUN_TIME -file $HEAP_PATH -m $M -g $G

            for SM in 'no-barrier' 'barrier' 'flush' 'nstore-no-barrier' 'nstore-barrier'; do
                rm -rf $HEAP_PATH
                ./dax_store.exe $M/$G/$SM -tc $TC -footMB $FOOT -rt $RUN_TIME -file $HEAP_PATH -m $M -g $G -s $SM
            done
        done
    done
done
