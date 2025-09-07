cmake_minimum_required(VERSION 3.30)

include(../cmake/configure_library.cmake)

option(BUILD_SHARED_LIBS "Build as shared library" OFF)

if (NOT SLANGC_EXECUTABLE)
    find_program(SLANGC_EXECUTABLE "slangc") # part of the vulkan SDK
endif()

find_package(Vulkan REQUIRED)
if (NOT Vulkan_FOUND) 
    message(FATAL_ERROR "Vulkan could not be found")
endif()
message(STATUS "Vulkan Include Directory: ${Vulkan_INCLUDE_DIR}")

add_subdirectory(third_party/vma)

configure_library(
    NAME liberay-vkren
    DEPS_PUBLIC liberay-util liberay-math liberay-res liberay-os glfw Vulkan::Vulkan VMA
    COMPILE_DEFINITIONS
      VULKAN_HPP_NO_EXCEPTIONS
      VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
      VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
      GLFW_INCLUDE_VULKAN
)
