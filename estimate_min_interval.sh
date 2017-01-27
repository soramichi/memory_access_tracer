#!/usr/bin/env bash

PWD_OLD=`pwd` # Note: do not use name $PWD, as it's automatically overwritten by the shell
PERF="/home/soramichi/src/linux-4.9.4/tools/perf/perf"
TARGET="/home/soramichi/src/cifar/svm/cifar_svm.py"
TARGET_DIR=`dirname $TARGET`
COUNTERS="r81D0:pp"

cd $TARGET_DIR # in case $TARGET accesses some data or libs with relative paths
for c in 400000 200000 100000 50000 25000 12500; do
    $PERF record -c $c -d -e $COUNTERS -o /dev/shm/perf.data.$c -- $TARGET > $PWD_OLD/$c.log 2>&1
done
