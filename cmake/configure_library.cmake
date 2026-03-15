include_guard(GLOBAL)

function(configure_library)
    # Parse arguments
    set(options SYSTEM)
    set(oneValueArgs TARGET_NAME)
    set(multiValueArgs
        PUBLIC_LINK_DEPS
        PRIVATE_LINK_DEPS

        PUBLIC_INCLUDE_DIRS
        PRIVATE_INCLUDE_DIRS

        PUBLIC_COMPILE_DEFINITIONS
        PRIVATE_COMPILE_DEFINITIONS

        POST_BUILD_COPY_DIRS
    )
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARGS_TARGET_NAME)
        message(FATAL_ERROR "configure_library requires TARGET_NAME")
    endif()

    set(target ${ARGS_TARGET_NAME})
    set(testsTarget "${ARGS_TARGET_NAME}_tests")

    set(PUBLIC_SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/public")
    set(PRIVATE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/private")
    set(TESTS_SOURCE_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/test")

    message(STATUS "Configuring library ${target}")
    list(APPEND CMAKE_MESSAGE_INDENT "  [${target}] ")

    file(GLOB_RECURSE PUBLIC_SOURCE_FILES CONFIGURE_DEPENDS
        "${PUBLIC_SOURCE_DIR}/*.cpp"
        "${PUBLIC_SOURCE_DIR}/*.hpp"
        "${PUBLIC_SOURCE_DIR}/*.h"
    )
    file(GLOB_RECURSE PRIVATE_SOURCE_FILES CONFIGURE_DEPENDS
        "${PRIVATE_SOURCE_DIR}/*.cpp"
        "${PRIVATE_SOURCE_DIR}/*.hpp"
        "${PRIVATE_SOURCE_DIR}/*.h"
    )
    file(GLOB_RECURSE TEST_SOURCE_FILES CONFIGURE_DEPENDS
        "${TESTS_SOURCE_DIR}/*.cpp"
        "${TESTS_SOURCE_DIR}/*.hpp"
        "${TESTS_SOURCE_DIR}/*.h"
    )

    add_library(${target})
    target_sources(${target}

        PUBLIC 
        ${PUBLIC_SOURCE_FILES}

        PRIVATE
        ${PRIVATE_SOURCE_FILES}
    )
    
    if(ARGS_SYSTEM)
        target_include_directories(${target} SYSTEM PUBLIC  ${PUBLIC_SOURCE_DIR})
        target_include_directories(${target} SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # enforce full include paths internally
    else()
        target_include_directories(${target} PUBLIC  ${PUBLIC_SOURCE_DIR})
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # enforce full include paths internally
    endif()

    if(ARGS_PUBLIC_INCLUDE_DIRS)
        if(ARGS_SYSTEM)
            target_include_directories(${target} SYSTEM PUBLIC ${ARGS_PUBLIC_INCLUDE_DIRS})
        else()
            target_include_directories(${target} PUBLIC ${ARGS_PUBLIC_INCLUDE_DIRS})
        endif()
    endif()

    if(ARGS_PRIVATE_INCLUDE_DIRS)
        if(ARGS_SYSTEM)
            target_include_directories(${target} SYSTEM PRIVATE ${ARGS_PRIVATE_INCLUDE_DIRS})
        else()
            target_include_directories(${target} PRIVATE ${ARGS_PRIVATE_INCLUDE_DIRS})
        endif()
    endif()

    # Dependencies
    if(ARGS_PUBLIC_LINK_DEPS)
        message(STATUS "Public link libraries dependencies: ${ARGS_PUBLIC_LINK_DEPS}")
        target_link_libraries(${target} PUBLIC ${ARGS_PUBLIC_LINK_DEPS})
    endif()
    if(ARGS_PRIVATE_LINK_DEPS)
        message(STATUS "Private link libraries dependencies: ${ARGS_PRIVATE_LINK_DEPS}")
        target_link_libraries(${target} PRIVATE ${ARGS_PRIVATE_LINK_DEPS})
    endif()

    # Compile definitions
    if(ARGS_PUBLIC_COMPILE_DEFINITIONS)
        message(STATUS "Public compile definitinos: ${ARGS_PUBLIC_COMPILE_DEFINITIONS}")
        target_compile_definitions(${ARGS_TARGET_NAME} PUBLIC ${ARGS_PUBLIC_COMPILE_DEFINITIONS})
    endif()
    if(ARGS_PRIVATE_COMPILE_DEFINITIONS)
        message(STATUS "Private compile definitinos: ${ARGS_PRIVATE_COMPILE_DEFINITIONS}")
        target_compile_definitions(${ARGS_TARGET_NAME} PRIVATE ${ARGS_PRIVATE_COMPILE_DEFINITIONS})
    endif()

    if(ERAY_GLOBAL_CXX_FLAGS)
        target_compile_options(${target} PRIVATE ${ERAY_GLOBAL_CXX_FLAGS})
    endif()

    # Tests configuration
    if(ERAY_BUILD_TESTS)
        message(STATUS "Configuring ${testsTarget} executable")
        if(NOT TEST_SOURCE_FILES)
            message(STATUS "No tests sources found for ${target}, ${testsTarget} will not be created")
        else()
            add_executable(${testsTarget})
            target_sources(${testsTarget} PUBLIC ${TEST_SOURCE_FILES})
            target_include_directories(${testsTarget} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # we want access to private
            target_link_libraries(${testsTarget} GTest::gtest_main ${target})

            gtest_discover_tests(${testsTarget})
        endif()
    endif()

    list(POP_BACK CMAKE_MESSAGE_INDENT)
endfunction()
