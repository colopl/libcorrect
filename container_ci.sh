#!/bin/sh

cd "$(dirname "${0}")"
rm -rf "build"
mkdir "build"
cd "build"
cmake ..
make -j"$(nproc)" shim
make check
