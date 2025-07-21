include_guard()
include(../cmake/configure_library.cmake)

option(BUILD_SHARED_LIBS "Build as shared library" OFF)

find_package(Slang CONFIG HINTS "$ENV{VULKAN_SDK}/lib/cmake")
find_package(Vulkan REQUIRED)

configure_library(
    NAME liberay-vkren
    LIBS liberay-util liberay-math Vulkan::Vulkan
    INCLUDE_DIRS ${Vulkan_INCLUDE_DIR}
)
