#!/bin/bash

make -s
if [ "$1" == "" ]; then
    ./run < test.txt
else
    ./run < $1
fi