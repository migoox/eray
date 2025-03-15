cmake_minimum_required(VERSION 3.20)

project(sandbox CXX)
message(STATUS "Configuring " ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/sandbox/*.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC liberay)
target_include_directories(${PROJECT_NAME}
                           PUBLIC . "${CMAKE_BINARY_DIR}/generated")

# Set compile options and properties of the target
target_compile_options(${PROJECT_NAME} PRIVATE ${PROJ_CXX_FLAGS})

# This is required when sandbox is linked with shared libraries
if(BUILD_SHARED_LIBS)
    target_link_options(${PROJECT_NAME} PRIVATE ${PROJ_EXE_LINKER_FLAGS})
endif()

# Copy assets folder to build directory
if(EXISTS "${PROJECT_SOURCE_DIR}/assets")
add_custom_target(copy_assets
  COMMAND ${CMAKE_COMMAND} -DASSETS_SOURCE_DIR=${PROJECT_SOURCE_DIR}/assets
                           -DASSETS_DEST_DIR=$<TARGET_FILE_DIR:${PROJECT_NAME}>
                           -P ${PROJECT_SOURCE_DIR}/../cmake/copy_assets.cmake
)
add_dependencies(${PROJECT_NAME} copy_assets)
endif()

