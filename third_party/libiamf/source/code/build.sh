#!/bin/bash

BUILD_LIBS=$PWD/build_libs
#1, build libiamf
make clean
cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS}  -DMULTICHANNEL_BINAURALIZER=ON -DHOA_BINAURALIZER=ON .
make
make install

#2, build test/tools/iamfdec

cd test/tools/iamfdec
make clean
cmake -DCMAKE_INSTALL_PREFIX=${BUILD_LIBS} -DMULTICHANNEL_BINAURALIZER=ON -DHOA_BINAURALIZER=ON .
make 
cd -
