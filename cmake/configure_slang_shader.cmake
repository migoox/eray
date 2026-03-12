include_guard(GLOBAL)

function(configure_slang_shader)
    set(options)
    set(singleValueArgs 
        TARGET_NAME # compilation target will be created with this name
        OUT_BINARY  # relative filename path
    )
    set(multiValueArgs 
        SHADER_SOURCES # relative shader source files
        ENTRY_POINTS   # shader entry points to be included in the binary output
    )

    cmake_parse_arguments(ARGS "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARGS_ENTRY_POINTS)
        message(FATAL_ERROR "No shader entry points provided for shader with name ${ARGS_TARGET_NAME}")
    endif()
    if (NOT ARGS_SHADER_SOURCES)
        message(FATAL_ERROR "No shader sources provided for shader with name ${ARGS_TARGET_NAME}")
    endif()

    if (NOT ARGS_OUT_BINARY)
        message(FATAL_ERROR "No output binary has been provided.")
    endif()

    get_filename_component(OUT_BINARY_FILENAME "${ARGS_OUT_BINARY}" NAME)
    get_filename_component(OUT_BINARY_DIR "${ARGS_OUT_BINARY}" DIRECTORY)

    if(NOT OUT_BINARY_FILENAME)
        message(FATAL_ERROR "${ARGS_OUT_BINARY} does not contain a filename!")
    endif()

    if (NOT SLANGC_EXECUTABLE)
        find_program(SLANGC_EXECUTABLE "slangc") # part of the vulkan SDK
    endif()
    if (NOT SLANGC_EXECUTABLE)
        message(FATAL_ERROR "Could not add slang shader target. The SLANGC_EXECUTABLE variable is not set.")
    endif() 

    set(ABS_SHADER_SOURCES "")
    foreach(SHADER_SOURCE ${ARGS_SHADER_SOURCES})
        list(APPEND ABS_SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}")
    endforeach()

    set(ENTRY_POINTS_SLANGC_ARGS "")
    foreach(ENTRY_POINT ${ARGS_ENTRY_POINTS})
        list(APPEND ENTRY_POINTS_SLANGC_ARGS "-entry")
        list(APPEND ENTRY_POINTS_SLANGC_ARGS "${ENTRY_POINT}")
    endforeach()

    file(MAKE_DIRECTORY "${OUT_BINARY_DIR}")

    add_custom_command(
          OUTPUT  "${ARGS_OUT_BINARY}"
          COMMAND ${SLANGC_EXECUTABLE} ${ABS_SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS_SLANGC_ARGS} -o ${ARGS_OUT_BINARY}
          DEPENDS ${ABS_SHADER_SOURCES}
          COMMENT "Compiling Slang Shaders"
    )
    add_custom_target(${ARGS_TARGET_NAME} DEPENDS "${ARGS_OUT_BINARY}")
    
    message(STATUS "Shader target ${ARGS_TARGET_NAME} and entry points {${ARGS_ENTRY_POINTS}} added. Shader output: ${ARGS_OUT_BINARY}")
endfunction()
