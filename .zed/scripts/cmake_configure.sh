#!/usr/bin/env bash
set -e

/usr/bin/cmake --preset "$(cat build/zed_cmake_tools/preset)"
