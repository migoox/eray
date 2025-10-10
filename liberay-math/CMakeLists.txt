project(liberay-math CXX C)

include_guard()
include(../cmake/deps_fetcher.cmake)
include(../cmake/configure_library.cmake)

option(BUILD_TESTS "Fetch GoogleTest and build tests" OFF)

configure_library(NAME liberay-math HEADER_ONLY)
