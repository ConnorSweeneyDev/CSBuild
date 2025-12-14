#!/bin/bash

./script/internal/bootstrap.sh || exit $?
./build/csb clean
