function(configure_library)
    # Parse arguments
    set(options HEADER_ONLY)
    set(oneValueArgs NAME)
    set(multiValueArgs LIBS COMPILE_DEFINITIONS)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    cmake_minimum_required(VERSION 3.20)
    set(CMAKE_CXX_STANDARD 23)

    project(${ARGS_NAME} CXX C)
    message(STATUS "Configuring " ${PROJECT_NAME})
    list(APPEND CMAKE_MESSAGE_INDENT "  ")

    if(ARGS_HEADER_ONLY)
        add_library(${PROJECT_NAME} INTERFACE)
        
        if(USE_SYSTEM_INCLUDE)
            target_include_directories(${PROJECT_NAME} SYSTEM INTERFACE 
                ${CMAKE_CURRENT_SOURCE_DIR} 
                "${CMAKE_BINARY_DIR}/generated"
            )
        else()
            target_include_directories(${PROJECT_NAME} INTERFACE 
                ${CMAKE_CURRENT_SOURCE_DIR} 
                "${CMAKE_BINARY_DIR}/generated"
            )
        endif()

        # Dependencies
        set(LIBS ${ARGS_LIBS})
        if(${TRACY_ENABLE})
            list(APPEND LIBS TracyClient)
        endif()
        message(STATUS "Requested libraries: ${LIBS}")
        target_link_libraries(${PROJECT_NAME} INTERFACE ${LIBS})

        # Compile definitions
        if(ARGS_COMPILE_DEFINITIONS)
            message(STATUS "Requested compile definitions: ${ARGS_COMPILE_DEFINITIONS}")
            target_compile_definitions(${PROJECT_NAME} INTERFACE ${ARGS_COMPILE_DEFINITIONS})
        endif()
    else()
        file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/liberay/*.cpp")

        if (NOT SOURCES)
            message(FATAL_ERROR "No source files detected for target: ${PROJECT_NAME}. If it's header-only library, consider using HEADER_ONLY option")
        endif()

        add_library(${PROJECT_NAME} ${SOURCES})
        
        if(USE_SYSTEM_INCLUDE)
            target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC 
                ${CMAKE_CURRENT_SOURCE_DIR} 
                "${CMAKE_BINARY_DIR}/generated"
            )
        else()
            target_include_directories(${PROJECT_NAME} PUBLIC 
                ${CMAKE_CURRENT_SOURCE_DIR} 
                "${CMAKE_BINARY_DIR}/generated"
            )
        endif()

        # Dependencies
        set(LIBS ${ARGS_LIBS})
        if(${TRACY_ENABLE})
            list(APPEND LIBS TracyClient)
        endif()
        message(STATUS "Dependencies: ${LIBS}")
        target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBS})

        # Compile definitions
        if(ARGS_COMPILE_DEFINITIONS)
            message(STATUS "Requested compile definitions: ${ARGS_COMPILE_DEFINITIONS}")
            target_compile_definitions(${PROJECT_NAME} PUBLIC ${ARGS_COMPILE_DEFINITIONS})
        endif()

        # Compiler and linker
        target_compile_options(${PROJECT_NAME} PRIVATE ${PROJ_CXX_FLAGS})
        target_link_options(${PROJECT_NAME} PRIVATE ${PROJ_SHARED_LINKER_FLAGS})
    endif()

    # Tests configuration
    if(BUILD_TESTS)
        message(STATUS "Configuring ${PROJECT_NAME}_tests executable")
        file(GLOB_RECURSE TESTS_SOURCES 
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
        )
        if(NOT TESTS_SOURCES)
            message(STATUS "No tests sources found, tests executable configuration stopped")
        elseif()
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
