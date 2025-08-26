cmake_minimum_required(VERSION 3.30)

find_package(Git)
include(cmake/find_repo_version.cmake)
find_repo_version(VERSION IS_STABLE)

project(
  eray 
  DESCRIPTION "Cross-platform family of libraries for 3D rendering"
  LANGUAGES CXX C
  VERSION ${VERSION})

set(CMAKE_CXX_STANDARD 23)

option(GENERATE_CLANGD_FILE
  "Generate the .clangd file for the host operating system" ON)
option(BUILD_SANDBOX "Build liberay sandbox executable" OFF)
option(BUILD_EXAMPLES "Build liberay example executables" OFF)
option(ENABLE_TRACY "Fetches and uses tracy for frame profiling" OFF)

set(VERSION_SUFFIX "")
if(IS_STABLE EQUAL 0)
  set(VERSION_SUFFIX "(unstable)")
endif()
message(STATUS "Version: ${CMAKE_PROJECT_VERSION}${VERSION_SUFFIX}")

# Generate the version header
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in
               ${CMAKE_BINARY_DIR}/generated/version/version.hpp)

# Set the binaries and libraries output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Use FindXXX.cmake from cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Include CTest in order to generate `DartConfiguration.tcl`
include(CTest)

if(GENERATE_CLANGD_FILE)
  include(cmake/generate_clangd_file.cmake)
  generate_clangd_file("${PROJ_CXX_FLAGS}")
endif()

set(PATCHES_DIR "${CMAKE_CURRENT_LIST_DIR}/patches")

if(TRACY_ENABLE)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/deps_fetcher.cmake")
    fetch_tracy()
endif()

add_subdirectory(liberay-util)
add_subdirectory(liberay-math)
add_subdirectory(liberay-res)
add_subdirectory(liberay-os)
add_subdirectory(liberay-driver)
add_subdirectory(liberay-vkren)

if(BUILD_SANDBOX) 
    add_subdirectory(sandbox)
endif()

if(BUILD_EXAMPLES) 
    add_subdirectory(examples/depth_buffer)
endif()
