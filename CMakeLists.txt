cmake_minimum_required(VERSION 3.20)

find_package(Git)
include(cmake/find_repo_version.cmake)
find_repo_version(VERSION IS_STABLE)

project(
  eray 
  DESCRIPTION "Cross-platform rendering library"
  LANGUAGES CXX C
  VERSION ${VERSION})

option(GENERATE_CLANGD_FILE
  "Generate the .clangd file for the host operating system" ON)
option(USE_LOCAL_INSTEAD_FETCH "Uses third_party folder to access the dependency repositories instead fetching the dependencies from remote" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_GLFW "Fetch and build GLFW. When OFF, CMake will look for proper GLFW version on your operating system." ON)
option(BUILD_ASSIMP "Fetch and build assimp. When OFF, CMake will look for proper assimp version on your operating system." ON)
option(BUILD_SANDBOX "Build liberay sandbox executable" OFF)
option(BUILD_TESTS "Fetch GoogleTest and build tests" OFF)
option(USE_SYSTEM_INCLUDE "Marks liberay includes as system, so they are excluded from the static analysis. Useful when using dori as a dependency." ON)

# Print version
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

# Include CTest in order to generate `DartConfiguration.tcl`
include(CTest)

if(GENERATE_CLANGD_FILE)
  include(cmake/generate_clangd_file.cmake)
  generate_clangd_file("${PROJ_CXX_FLAGS}")
endif()

# Prepare dependencies
if(NOT BUILD_GLFW)
  find_package(GLFW 3.4)
endif()

if(NOT BUILD_ASSIMP)
  find_package(assimp 5.4.3)
endif()

add_subdirectory(cmake/dependencies)
add_subdirectory(third_party/stb_image)

# eray library 
add_subdirectory(liberay-util)
add_subdirectory(liberay-math)
add_subdirectory(liberay-res)
add_subdirectory(liberay-os)
add_subdirectory(liberay-driver)
if(BUILD_SANDBOX) 
    add_subdirectory(sandbox)
endif()
