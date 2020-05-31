#!/bin/bash

[ ! -d xcode ] && mkdir xcode 

cd xcode
rm -rf CMakeCache.txt
cmake -G Xcode -DCMAKE_IGNORE_PATH="/usr/local/lib;/usr/local/include" -DCMAKE_INSTALL_PREFIX=~/Library/Frameworks ..
