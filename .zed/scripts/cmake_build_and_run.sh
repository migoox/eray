#!/usr/bin/env bash
set -e
mkdir -p build/zed_cmake_tools

if [ ! -e "build/zed_cmake_tools/target" ]; then
    .zed/scripts/cmake_full_configure.sh
fi
.zed/scripts/cmake_build.sh
build/zed_cmake_tools/out # uses symlink
