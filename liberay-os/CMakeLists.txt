cmake_minimum_required(VERSION 3.20)
include(../cmake/deps_fetcher.cmake)
include(../cmake/configure_library.cmake)

option(BUILD_GLFW "Fetch and build GLFW. When OFF, CMake will look for proper GLFW version on host operating system." ON)
option(BUILD_TESTS "Fetch GoogleTest and build tests" OFF)
option(USE_SYSTEM_INCLUDE "Marks liberay includes as system, so they are excluded from the static analysis. Useful when using eray as a dependency." ON)
option(BUILD_SHARED_LIBS "Build as shared library" OFF)

if(BUILD_GLFW)
  fetch_glfw()
else()
  find_package(GLFW 3.4)
endif()
fetch_imgui()
fetch_nfd()

if(BUILD_TESTS)
    fetch_googletest()
endif()

configure_library(
    NAME liberay-os 
    DEPS_PUBLIC glfw imgui nfd liberay-util liberay-math liberay-driver
    # GLFW_INCLUDE_NONE makes the GLFW header not include any OpenGL or OpenGL ES
    # API header. This is useful in combination with an extension loading library
    # (GLAD in our case).
    COMPILE_DEFINITIONS GLFW_INCLUDE_NONE GLFW_INCLUDE_VULKAN
)
