#!/usr/bin/env bash
set -e
mkdir -p build/zed_cmake_tools

USER_CMAKE_PRESET=$(/usr/bin/cmake --list-presets | grep -E '"' | sed 's/"//g' | sed 's/ //g' | fzf)
echo "$USER_CMAKE_PRESET" > build/zed_cmake_tools/preset
