cmake_minimum_required(VERSION 3.30)

project(
  eray
  DESCRIPTION "Cross-platform rendering engine"
  LANGUAGES CXX C
)

# Generate options
option(ERAY_GENERATE_CLANGD_FILE
  "Generate the .clangd file basing on ERAY_GLOBAL_CXX_FLAGS and compiler" ON)
option(ERAY_GENERATE_PCH
  "Generate precompiled headers" ON)

# Build options
option(ERAY_BUILD_SANDBOX "Build liberay sandbox executable" ON)
option(ERAY_BUILD_EXAMPLES "Build liberay example executables" ON)
option(ERAY_BUILD_VULKAN_RHI "Build vulkan Rendering Hardware Interface" ON)
option(ERAY_BUILD_DX12_RHI "Build DirectX12 Rendering Hardware Interface" OFF) # not supported yet
option(ERAY_BUILD_TESTS "Build tests" ON)

# Feature flags
option(ERAY_ENABLE_PROFILING "Enables profiling with tracy" OFF)

# CMake module
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Git)
include(find_repo_info)
find_repo_info(ERAY_VERSION)
message("Build version: ${ERAY_VERSION}")

# Generate build config header
set(ERAY_ABSOLUTE_BUILD_PATH "${CMAKE_CURRENT_BINARY_DIR}")
set(ERAY_GLOBAL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(ERAY_GLOBAL_GENERATED_CONFIG_PATH "${ERAY_GLOBAL_GENERATED_DIR}/build/config.hpp")
configure_file(
    cmake/config.hpp.in
    ${ERAY_GLOBAL_GENERATED_CONFIG_PATH}
)

# Include cmake modules
if(ERAY_BUILD_TESTS)
    include(CTest)
    include(GoogleTest)
    enable_testing()
endif()

include(configure_library)
include(configure_executable)
include(configure_slang_shader)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Generate clangd file
if(ERAY_GENERATE_CLANGD_FILE)
    include(generate_clangd_file)
    generate_clangd_file("${ERAY_GLOBAL_CXX_FLAGS}")
endif()

# Precompiled headers
if(ERAY_GENERATE_PCH)
    add_library(eray_pch INTERFACE)
    target_precompile_headers(eray_pch 
        INTERFACE 
        <unordered_map>
        <vector>
        # TODO: Add precompile headers
    )
endif()

add_subdirectory(eray)

if(BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

