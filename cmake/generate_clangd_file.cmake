function(generate_clangd_file COMPILER_FLAGS)
  if(CMAKE_HOST_UNIX)
    set(CLANGD_COMPILER_STANDARD "-std=c++23")
  elseif(CMAKE_HOST_WIN32)
    set(CLANGD_COMPILER_STANDARD "/clang:-std=c++23")
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CLANGD_COMPILATION_DATABASE "build/gnu-debug/")
  else()
  set(CLANGD_COMPILATION_DATABASE "build/gnu-release/")
  endif()

  string(CONCAT CLANGD_FILE_CONTENT 
    "CompileFlags:\n"
    "  CompilationDatabase: ${CLANGD_COMPILATION_DATABASE}\n"
    "  Add:\n"
    "    - "
    ${CLANGD_COMPILER_STANDARD}
    "\n")
  
    foreach(FLAG ${COMPILER_FLAGS})
      string(CONCAT CLANGD_FILE_CONTENT 
      "${CLANGD_FILE_CONTENT}"
      "    - "
      ${FLAG}
      "\n")
    endforeach()
    

  file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/.clangd "${CLANGD_FILE_CONTENT}")
endfunction()
