#!/bin/bash

MNTPOINT="/mnt/pmem12"
HEAP_PATH="$MNTPOINT/heap"
RUN_TIME=60 # Sec
FOOT=81920 # 80 GB

TEST1=`mount | grep $MNTPOINT`
TEST2=`echo $TEST1 | grep dax`

DEV=`lsblk | grep $MNTPOINT | awk {' print $1 '}`

if [ -z "$TEST1" ]; then
    echo "$MNTPOINT not mounted"
    exit -1
fi

if [ -z "$TEST2" ]; then
    echo "$MNTPOINT not mounted as DAX"
    exit -1
fi

echo "Running benchmark at $HEAP_PATH, device $DEV"

for TC in 1 2 4 8 16 32 64; do
    for M in 'rnd' 'seq'; do
        for G in 64 128 256 512 1024 2048 4096 8192; do
            rm -rf $HEAP_PATH
            ./dax_load.exe $M/$G -tc $TC -footMB $FOOT -rt $RUN_TIME -file $HEAP_PATH -m $M -g $G

            for SM in 'no-barrier' 'barrier' 'flush' 'nstore-no-barrier' 'nstore-barrier'; do
                rm -rf $HEAP_PATH
                ./dax_store.exe $M/$G/$SM -tc $TC -footMB $FOOT -rt $RUN_TIME -file $HEAP_PATH -m $M -g $G -s $SM
            done
        done
    done
done
