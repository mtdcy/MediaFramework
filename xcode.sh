#!/bin/bash

if [ ! -d xcode ]; then
    mkdir xcode 
fi

cd xcode

cmake -G Xcode -DCMAKE_IGNORE_PATH="/usr/local/lib;/usr/local/include" -DCMAKE_INSTALL_PREFIX=~/Library/Frameworks ..
