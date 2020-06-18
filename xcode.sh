#!/bin/bash

[ -d xcode ] && rm -rf xcode 

mkdir xcode && 
cd xcode
cmake -G Xcode -DCMAKE_IGNORE_PATH="/usr/local/lib;/usr/local/include" -DCMAKE_INSTALL_PREFIX=~/Library/Frameworks ..
