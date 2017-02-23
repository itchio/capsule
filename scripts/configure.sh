#!/bin/sh -xe

mkdir -p build32
(cd build32 && cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-Linux32.cmake ..)

mkdir -p build64
(cd build64 && cmake -DCMAKE_BUILD_TYPE=Debug ..)
