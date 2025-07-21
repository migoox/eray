include_guard()
include(../cmake/deps_fetcher.cmake)
include(../cmake/configure_library.cmake)

option(BUILD_TESTS "Fetch GoogleTest and build tests" OFF)

add_subdirectory(third_party/glad)

configure_library(
    NAME liberay-driver 
    LIBS glad liberay-util liberay-math
)


