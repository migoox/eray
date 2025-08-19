cmake_minimum_required(VERSION 3.20)
include(../cmake/configure_library.cmake)

option(BUILD_ASSIMP "Fetch and build assimp. When OFF, CMake will look for proper assimp version on host operating system." ON)
option(BUILD_SHARED_LIBS "Build as shared library" OFF)

if(BUILD_ASSIMP)
  fetch_assimp()
else()
  find_package(assimp 5.4)
endif()

add_subdirectory(third_party/stb_image)

configure_library(
    NAME liberay-res liberay-math
    DEPS_PUBLIC stb_image liberay-util
)
