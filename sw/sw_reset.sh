#!/bin/bash
set -o xtrace
rm -rf build
mkdir build && cd build
cmake ../ -DTARGET_DIR=/home/jiayli/projects/coyote/sw/examples/dedup_bench1
make