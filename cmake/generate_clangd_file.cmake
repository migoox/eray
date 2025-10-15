include_guard(GLOBAL)

function(generate_clangd_file COMPILER_FLAGS)
  if(CMAKE_HOST_UNIX)
    set(CLANGD_COMPILER_STANDARD "-std=c++23")
  elseif(CMAKE_HOST_WIN32)
    set(CLANGD_COMPILER_STANDARD "/clang:-std=c++23")
  endif()

  string(CONCAT CLANGD_FILE_CONTENT 
    "CompileFlags:\n"
    "  CompilationDatabase: ${CMAKE_BINARY_DIR}\n"
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

  string(CONCAT CLANGD_FILE_CONTENT 
    "${CLANGD_FILE_CONTENT}"
    "  Remove:\n"
    "    - -fmodule*\n"
    "    - -fdeps*\n"
    "\n")
    

  file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/.clangd "${CLANGD_FILE_CONTENT}")
endfunction()
