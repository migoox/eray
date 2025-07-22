cmake_minimum_required(VERSION 3.30)

include(../cmake/configure_library.cmake)

option(BUILD_SHARED_LIBS "Build as shared library" OFF)

find_package(Slang CONFIG HINTS "$ENV{VULKAN_SDK}/lib/cmake")
find_package(Vulkan REQUIRED)

configure_library(
    NAME liberay-vkren
    DEPS_PUBLIC liberay-util liberay-math  
    COMPILE_DEFINITIONS
      VULKAN_HPP_NO_EXCEPTIONS
      VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
      VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)
