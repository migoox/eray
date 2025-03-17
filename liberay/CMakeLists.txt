cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 23)

project(liberay CXX C)
message(STATUS "Configuring " ${PROJECT_NAME})
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/liberay/*.cpp"
)

add_library(${PROJECT_NAME} ${SOURCES})

# GLFW_INCLUDE_NONE makes the GLFW header not include any OpenGL or OpenGL ES
# API header. This is useful in combination with an extension loading library
# (GLAD in our case).
target_compile_definitions(${PROJECT_NAME} PUBLIC GLFW_INCLUDE_NONE)

target_link_libraries(${PROJECT_NAME} PUBLIC glad glfw imgui)

if (USE_SYSTEM_INCLUDE)
    target_include_directories(${PROJECT_NAME}
        SYSTEM PUBLIC . "${CMAKE_BINARY_DIR}/generated")
else()
    target_include_directories(${PROJECT_NAME}
        PUBLIC . "${CMAKE_BINARY_DIR}/generated")
endif()

target_compile_options(${PROJECT_NAME} PRIVATE ${PROJ_CXX_FLAGS})
target_link_options(${PROJECT_NAME} PRIVATE ${PROJ_SHARED_LINKER_FLAGS})

if(BUILD_TESTS)
  enable_testing()

  file(GLOB_RECURSE TESTS_SOURCES 
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
  )

  add_executable(
    "${PROJECT_NAME}_tests"
    ${TESTS_SOURCES} 
  )
  target_link_libraries(
    "${PROJECT_NAME}_tests"
    GTest::gtest_main
    ${PROJECT_NAME}
  )

  include(CTest)
  include(GoogleTest)
  gtest_discover_tests("${PROJECT_NAME}_tests")
endif()
