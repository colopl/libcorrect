#!/bin/sh

cd "$(dirname "${0}")"

rm -rf "build"
mkdir "build"
cd "build"
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j"$(nproc)" shim
make check
cd -

rm -rf "build"
mkdir "build"
cd "build"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)" shim
make check
cd -
