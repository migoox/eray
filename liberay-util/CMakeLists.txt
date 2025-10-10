project(liberay-util CXX C)

cmake_minimum_required(VERSION 3.20)

include(../cmake/deps_fetcher.cmake)
include(../cmake/configure_library.cmake)

option(BUILD_TESTS "Fetch GoogleTest and build tests" OFF)
option(BUILD_SHARED_LIBS "Build as shared library" OFF)

if(BUILD_TESTS)
    fetch_googletest()
endif()

configure_library(
    NAME liberay-util
)
