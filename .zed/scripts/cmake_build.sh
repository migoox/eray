#!/usr/bin/env bash
set -e

/usr/bin/cmake --build --preset "$(cat build/zed_cmake_tools/preset)" --target "$(cat build/zed_cmake_tools/target)"

TARGET_BIN=$(cat build/zed_cmake_tools/target_bin)
ln -sf $TARGET_BIN build/zed_cmake_tools/out
