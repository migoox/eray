project(liberay-glren CXX C)

cmake_minimum_required(VERSION 3.20)
include(../cmake/deps_fetcher.cmake)
include(../cmake/configure_library.cmake)

option(BUILD_TESTS "Fetch GoogleTest and build tests" OFF)

add_subdirectory(third_party/glad)

configure_library(
    NAME liberay-glren 
    DEPS_PUBLIC glad imgui glfw liberay-util liberay-math liberay-os
)


