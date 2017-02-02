#!/usr/bin/env bash

# args: $0 comm, $1 counters, $2 target program, $[3-N] arguments to the target program

PWD_OLD=`pwd`

# in case the target program uses data relative to its own pwd
TARGET_DIR=`dirname $2` # assume $2 is the program name

# for vector.o and hash.o
cd $TARGET_DIR
LD_LIBRARY_PATH=$PWD_OLD $PWD_OLD/mat $@
