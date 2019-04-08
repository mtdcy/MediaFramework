#!/bin/bash

if [ ! -d xcode ]; then
    mkdir xcode 
fi

cd xcode

cmake -G Xcode -DCMAKE_INSTALL_PREFIX=~/Library/Frameworks ..
