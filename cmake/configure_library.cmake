include_guard(GLOBAL)

function(configure_library)
    # Parse arguments
    set(options HEADER_ONLY)
    set(oneValueArgs NAME)
    set(multiValueArgs DEPS_PUBLIC DEPS_SYSTEM INCLUDE_DIRS COMPILE_DEFINITIONS)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    message(STATUS "Configuring " ${PROJECT_NAME})
    list(APPEND CMAKE_MESSAGE_INDENT "  [${PROJECT_NAME}] ")

    if(ARGS_HEADER_ONLY)
        add_library(${PROJECT_NAME} INTERFACE)
        
        if(USE_SYSTEM_INCLUDE)
            target_include_directories(${PROJECT_NAME} SYSTEM INTERFACE 
                ${CMAKE_CURRENT_SOURCE_DIR} 
                "${CMAKE_BINARY_DIR}/generated"
                "${ARGS_INCLUDE_DIRS}"
            )
        else()
            target_include_directories(${PROJECT_NAME} INTERFACE 
                ${CMAKE_CURRENT_SOURCE_DIR} 
                "${CMAKE_BINARY_DIR}/generated"
                "${ARGS_INCLUDE_DIRS}"
            )
        endif()

        # Dependencies
        set(DEPS_PUBLIC ${ARGS_DEPS_PUBLIC})
        if(${TRACY_ENABLE})
            list(APPEND DEPS_PUBLIC TracyClient)
        endif()
        message(STATUS "Requested libraries: ${DEPS_PUBLIC}")
        target_link_libraries(${PROJECT_NAME} INTERFACE ${DEPS_PUBLIC})
        target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_23)

        # Compile definitions
        if(ARGS_COMPILE_DEFINITIONS)
            message(STATUS "Default compile definitions: ERAY_ABS_BUILD_PATH=${CMAKE_SOURCE_DIR}")
            message(STATUS "Requested compile definitions: ${ARGS_COMPILE_DEFINITIONS}")
            target_compile_definitions(${PROJECT_NAME} INTERFACE 
                ${ARGS_COMPILE_DEFINITIONS}
                ERAY_ABS_BUILD_PATH="${CMAKE_SOURCE_DIR}"
            )
        else()
            message(STATUS "Default compile definitions: ERAY_ABS_BUILD_PATH=${CMAKE_SOURCE_DIR}")
            target_compile_definitions(${PROJECT_NAME} INTERFACE 
                ERAY_ABS_BUILD_PATH="${CMAKE_SOURCE_DIR}"
            )
        endif()
    else()
        file(GLOB_RECURSE SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/liberay/*.cpp"
        )

        file(GLOB_RECURSE HEADERS
            "${CMAKE_CURRENT_SOURCE_DIR}/liberay/*.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/liberay/*.h"
        )

        add_library(${PROJECT_NAME}
            ${SOURCES}
            ${HEADERS}
        )
        
        if(USE_SYSTEM_INCLUDE)
            target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC 
                ${CMAKE_CURRENT_SOURCE_DIR} 
                "${CMAKE_BINARY_DIR}/generated"
                "${ARGS_INCLUDE_DIRS}"
            )
        else()
            target_include_directories(${PROJECT_NAME} PUBLIC 
                ${CMAKE_CURRENT_SOURCE_DIR} 
                "${CMAKE_BINARY_DIR}/generated"
                "${ARGS_INCLUDE_DIRS}"
            )
        endif()

        # Dependencies
        set(DEPS_PUBLIC ${ARGS_DEPS_PUBLIC})
        if(${TRACY_ENABLE})
            list(APPEND DEPS_PUBLIC TracyClient)
        endif()
        message(STATUS "Dependencies: ${DEPS_PUBLIC}")
        if (ARGS_INCLUDE_DIRS)
            message(STATUS "Requested include directories: ${ARGS_INCLUDE_DIRS}")
        endif()
        target_link_libraries(${PROJECT_NAME} PUBLIC ${DEPS_PUBLIC})

        # Compile definitions
        message(STATUS "Default compile definitions: ERAY_ABS_BUILD_PATH=${CMAKE_SOURCE_DIR}")
        target_compile_definitions(${PROJECT_NAME} PRIVATE 
            ERAY_ABS_BUILD_PATH="${CMAKE_SOURCE_DIR}"
        )
        if(ARGS_COMPILE_DEFINITIONS)
            message(STATUS "Requested compile definitions: ${ARGS_COMPILE_DEFINITIONS}")
            target_compile_definitions(${PROJECT_NAME} PUBLIC ${ARGS_COMPILE_DEFINITIONS})
        endif()

        # Compiler and linker
        target_compile_options(${PROJECT_NAME} PRIVATE ${GLOBAL_CXX_FLAGS})
        target_link_options(${PROJECT_NAME} PRIVATE ${PROJ_SHARED_LINKER_FLAGS})
        target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
    endif()

    # Tests configuration
    if(BUILD_TESTS)
        # Include CTest in order to generate `DartConfiguration.tcl`
        include(CTest)

        message(STATUS "Configuring ${PROJECT_NAME}_tests executable")
        file(GLOB_RECURSE TESTS_SOURCES 
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
        )
        if(NOT TESTS_SOURCES)
            message(STATUS "No tests sources found, tests executable configuration stopped")
        else()
            enable_testing()

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
    endif()

    list(POP_BACK CMAKE_MESSAGE_INDENT)
endfunction()
