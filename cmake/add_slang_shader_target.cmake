include_guard()

# Use relative paths for sources
function(add_slang_shader_target TARGET)
    set(multiValueArgs SOURCES ENTRY_POINTS)
    set(singleValueArgs SHADERS_DIR OUT_BINARY_NAME)
    cmake_parse_arguments(ARGS "SHADER" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT ARGS_ENTRY_POINTS)
        message(FATAL_ERROR "No shader entry points provided for shader with name ${TARGET}")
    endif()
    if (NOT ARGS_SOURCES)
        message(FATAL_ERROR "No shader sources provided for shader with name ${TARGET}")
    endif()
    if (NOT SLANGC_EXECUTABLE)
        message(FATAL_ERROR "Could not add slang shader target. The SLANGC_EXECUTABLE variable is not set.")
    endif()

    set(SOURCES "")
    foreach(SOURCE ${ARGS_SOURCES})
        list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE}")
    endforeach()
    
    set (SHADERS_DIR "")
    if (NOT ARGS_SHADERS_DIR)
        set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
    elseif()
        set (SHADERS_DIR ${ARGS_SHADERS_DIR})
    endif()

    if(NOT ARGS_OUT_BINARY_NAME)
        set(OUT_BINARY_NAME "${TARGET}")
    else()
        set(OUT_BINARY_NAME "${ARGS_OUT_BINARY_NAME}")
    endif()
    set(OUT_BINARY "${SHADERS_DIR}/${OUT_BINARY_NAME}.spv")

    set(ENTRY_POINTS_SLANGC_ARGS "")
    foreach(ENTRY_POINT ${ARGS_ENTRY_POINTS})
        list(APPEND ENTRY_POINTS_SLANGC_ARGS "-entry")
        list(APPEND ENTRY_POINTS_SLANGC_ARGS "${ENTRY_POINT}")
    endforeach()

    add_custom_command (
          OUTPUT ${SHADERS_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    )

    add_custom_command (
          OUTPUT  "${OUT_BINARY}"
          COMMAND ${SLANGC_EXECUTABLE} ${SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS_SLANGC_ARGS} -o ${OUT_BINARY_NAME}.spv
          WORKING_DIRECTORY ${SHADERS_DIR}
          DEPENDS ${SHADERS_DIR} ${SOURCES}
          COMMENT "Compiling Slang Shaders"
          VERBATIM
    )

    add_custom_target(${TARGET} DEPENDS "${OUT_BINARY}")
    
    message(STATUS "Shader target ${TARGET} and entry points ${ARGS_ENTRY_POINTS} added. Shader output: ${OUT_BINARY}")

endfunction()
