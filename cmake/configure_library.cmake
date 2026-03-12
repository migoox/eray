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
        message(FATAL_ERROR "configure_executable requires TARGET_NAME")
    endif()

    set(target ${ARGS_TARGET_NAME})

    message(STATUS "Configuring library ${target}")
    list(APPEND CMAKE_MESSAGE_INDENT "  [${target}] ")

    set(PUBLIC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/public")
    file(GLOB_RECURSE PUBLIC_SOURCE_FILES CONFIGURE_DEPENDS
        "${PUBLIC_SOURCE_DIR}/*.cpp"
        "${PUBLIC_SOURCE_DIR}/*.hpp"
        "${PUBLIC_SOURCE_DIR}/*.h"
    )

    set(PRIVATE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/private")
    file(GLOB_RECURSE PUBLIC_SOURCE_FILES CONFIGURE_DEPENDS
        "${PRIVATE_SOURCE_DIR}/*.cpp"
        "${PRIVATE_SOURCE_DIR}/*.hpp"
        "${PRIVATE_SOURCE_DIR}/*.h"
    )

    add_library(${target})
    target_sources(${target}

        PUBLIC 
        ${PUBLIC_SOURCE_DIR}

        PRIVATE
        ${PRIVATE_SOURCE_DIR}
    )
    
    if(ARGS_SYSTEM)
        target_include_directories(${target} SYSTEM PUBLIC ${PUBLIC_SOURCE_DIR})
        target_include_directories(${target} SYSTEM PRIVATE ${PRIVATE_SOURCE_DIR})
    else()
        target_include_directories(${target} PUBLIC ${PUBLIC_SOURCE_DIR})
        target_include_directories(${target} PRIVATE ${PRIVATE_SOURCE_DIR})
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
        target_compile_definitions(${ARGS_TARGET_NAME} PUBLIC ${ARGS_COMPILE_DEFINITIONS})
    endif()
    if(ARGS_PRIVATE_COMPILE_DEFINITIONS)
        message(STATUS "Private compile definitinos: ${ARGS_PRIVATE_COMPILE_DEFINITIONS}")
        target_compile_definitions(${ARGS_TARGET_NAME} PRIVATE ${ARGS_COMPILE_DEFINITIONS})
    endif()

    target_compile_options(${ARGS_TARGET_NAME} PRIVATE ${GLOBAL_CXX_FLAGS})

    # Tests configuration
    if(ERAY_BUILD_TESTS)
        # Include CTest in order to generate `DartConfiguration.tcl`
        include(CTest)

        message(STATUS "Configuring ${ARGS_TARGET_NAME}_tests executable")
        file(GLOB_RECURSE TESTS_SOURCES 
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
        )
        if(NOT TESTS_SOURCES)
            message(STATUS "No tests sources found, tests executable configuration stopped")
        else()
            enable_testing()

            add_executable(
                "${ARGS_TARGET_NAME}_tests"
                ${TESTS_SOURCES} 
            )
            target_link_libraries(
                "${ARGS_TARGET_NAME}_tests"
                GTest::gtest_main
                ${ARGS_TARGET_NAME}
            )

            include(CTest)
            include(GoogleTest)
            gtest_discover_tests("${ARGS_TARGET_NAME}_tests")
        endif()
    endif()

    list(POP_BACK CMAKE_MESSAGE_INDENT)
endfunction()
