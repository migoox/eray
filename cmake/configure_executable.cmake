include_guard(GLOBAL)

function(configure_executable)
    set(options)
    set(oneValueArgs TARGET_NAME)
    set(multiValueArgs
        PUBLIC_LINK_DEPS
        PUBLIC_INCLUDE_DIRS
        PUBLIC_COMPILE_DEFINITIONS
        POST_BUILD_COPY_DIRS
    )

    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARGS_TARGET_NAME)
        message(FATAL_ERROR "configure_executable requires TARGET_NAME")
    endif()
    if (NOT ERAY_GLOBAL_GENERATED_DIR)
        message(FATAL_ERROR "ERAY_GLOBAL_GENERATED_DIR not defined")
    endif()

    set(target ${ARGS_TARGET_NAME})

    message(STATUS "Configuring executable ${target}")
    list(APPEND CMAKE_MESSAGE_INDENT "  [${target}] ")

    set(PUBLIC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/public")
    file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
        "${PUBLIC_SOURCE_DIR}/*.cpp"
        "${PUBLIC_SOURCE_DIR}/*.hpp"
        "${PUBLIC_SOURCE_DIR}/*.h"
    )

    add_executable(${target} ${SOURCE_FILES})

    if(ARGS_PUBLIC_LINK_DEPS)
        message(STATUS "Public link libraries dependencies: ${ARGS_PUBLIC_LINK_DEPS}")
        target_link_libraries(${target} PUBLIC ${ARGS_PUBLIC_LINK_DEPS})
    endif()

    target_include_directories(${target} PUBLIC ${PUBLIC_SOURCE_DIR})
    target_include_directories(${target} PRIVATE ${ERAY_GLOBAL_GENERATED_DIR})

    if(ARGS_PUBLIC_INCLUDE_DIRS)
        target_include_directories(${target} PUBLIC ${ARGS_PUBLIC_INCLUDE_DIRS})
    endif()
    message(STATUS "Public include directories: ${PUBLIC_SOURCE_DIR} ${ARGS_PUBLIC_INCLUDE_DIRS}")

    if(ARGS_PUBLIC_COMPILE_DEFINITIONS)
        message(STATUS "Public compile definitinos: ${ARGS_PUBLIC_COMPILE_DEFINITIONS}")
        target_compile_definitions(${target} PUBLIC ${ARGS_PUBLIC_COMPILE_DEFINITIONS})
    endif()

    if(ERAY_GLOBAL_CXX_FLAGS)
        target_compile_options(${target} PRIVATE ${ERAY_GLOBAL_CXX_FLAGS})
    endif()

    if(ARGS_POST_BUILD_COPY_DIRS)
        foreach(DIR ${ARGS_POST_BUILD_COPY_DIRS})
            set(ABS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${DIR}")
            message(STATUS "Post build copy directory: ${ABS_DIR}")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${ABS_DIR}
                    "$<TARGET_FILE_DIR:${target}>/${DIR}"
            )
        endforeach()
    endif()

    list(POP_BACK CMAKE_MESSAGE_INDENT)
endfunction()
