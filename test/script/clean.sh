#!/bin/bash

mkdir -p build
g++ -std=c++20 -O2 -o build/csb csb.cpp
if [ $? -ne 0 ]; then exit $?; fi
./build/csb clean
