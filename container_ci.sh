#!/bin/sh -eu

cd "$(dirname "${0}")"

for COMPILER in "$(which "gcc")" "$(which "clang")"; do
    for BUILD_TYPE in "Debug" "Release"; do
        rm -rf "build"
        mkdir "build"
        cd "build"
            cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_C_COMPILER="${COMPILER}" -DENABLE_LIBCORRECT_TEST=ON
            make -j"$(nproc)" shim test_runners
            cd "tests"
                ctest -C "${BUILD_TYPE}" --output-on-failure
            cd -
        cd -
    done
done
