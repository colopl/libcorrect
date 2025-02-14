#!/bin/sh

cd "$(dirname "${0}")"

for COMPILER in "$(which "gcc")" "$(which "clang")"; do
    for BUILD_TYPE in "Debug" "Release"; do
        rm -rf "build"
        mkdir "build"
        cd "build"
            cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_C_COMPILER="${COMPILER}"
            make -j"$(nproc)" shim
            make check
        cd -
    done
done
