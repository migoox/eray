#!/usr/bin/env bash
set -e
mkdir -p build/zed_cmake_tools

.zed/scripts/cmake_choose_preset.sh
.zed/scripts/cmake_configure.sh
.zed/scripts/cmake_choose_target.sh

USER_CMAKE_PRESET=$(cat build/zed_cmake_tools/preset)
USER_CMAKE_TARGET=$(cat build/zed_cmake_tools/target)

(
    set +e # ignore errors

    TARGET_BIN=$(cat build/$USER_CMAKE_PRESET/build.ninja | grep -E "TARGET_FILE = .*vk_triangle" | sed 's/TARGET_FILE//' | sed 's/=//' | sed 's/ //g')
    TARGET_BIN="$ZED_WORKTREE_ROOT/build/$USER_CMAKE_PRESET/$TARGET_BIN"
    echo "$TARGET_BIN" > build/zed_cmake_tools/target_bin

    BOLD="\e[1m"
    GREEN="\e[32m"
    CYAN="\e[36m"
    YELLOW="\e[33m"
    RESET="\e[0m"

    echo -e "${BOLD}${CYAN}⏵ Task Summary:${RESET}"
    echo -e "    ${BOLD}${YELLOW}Preset:${RESET} ${GREEN}$USER_CMAKE_PRESET${RESET}"
    echo -e "    ${BOLD}${YELLOW}Target:${RESET} ${GREEN}$USER_CMAKE_TARGET${RESET}"
    echo -e "    ${BOLD}${YELLOW}Target binary file:${RESET} ${GREEN}$TARGET_BIN${RESET}"
)
