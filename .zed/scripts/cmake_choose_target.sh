#!/usr/bin/env bash
set -e
mkdir -p build/zed_cmake_tools

USER_CMAKE_PRESET=$(cat build/zed_cmake_tools/preset)
USER_CMAKE_TARGET=$(/usr/bin/cmake --build --preset "$USER_CMAKE_PRESET" --target help | grep -Ev "/|__|\.a|\.ninja|\.cmake|help|clean" | sed 's/: phony//' | fzf)

echo "$USER_CMAKE_TARGET" > build/zed_cmake_tools/target
