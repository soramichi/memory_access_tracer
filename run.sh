#!/usr/bin/env bash

PWD_OLD=`pwd`

# in case the target program uses data relative to its own pwd
TARGET_DIR=`dirname $1` # assume $1 is the program name

# for vector.o and hash.o
cd $TARGET_DIR
LD_LIBRARY_PATH=$PWD_OLD $PWD_OLD/mat $@
