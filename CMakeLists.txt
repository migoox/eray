cmake_minimum_required(VERSION 3.30)

project(
  eray
  DESCRIPTION "Cross-platform rendering engine"
  LANGUAGES CXX C
)

# Generate options
option(ERAY_GENERATE_CLANGD_FILE
  "Generate the .clangd file for the host operating system" ON)
option(ERAY_GENERATE_PCH
  "Generates and uses precompiled header" ON)

# Build options
option(ERAY_BUILD_SANDBOX "Build liberay sandbox executable" ON)
option(ERAY_BUILD_EXAMPLES "Build liberay example executables" ON)
option(ERAY_BUILD_VULKAN_RHI "Build vulkan Rendering Hardware Interface" ON)
option(ERAY_BUILD_DX12_RHI "Build DirectX12 Rendering Hardware Interface" OFF) # not supported yet

# Feature flags
option(ERAY_ENABLE_PROFILING "Enables profiling with tracy" OFF)

# CMake module
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Include CTest in order to generate `DartConfiguration.tcl`
include(CTest)

include(configure_executable)
include(configure_slang_shader)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(GENERATE_CLANGD_FILE)
    include(cmake/generate_clangd_file.cmake)
    generate_clangd_file("${GLOBAL_CXX_FLAGS}")
endif()

add_subdirectory(eray)

if(BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

#if(BUILD_EXAMPLES)
#    add_subdirectory(examples)
#endif()

