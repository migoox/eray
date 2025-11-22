include(../../cmake/add_slang_shader_target.cmake)
include(../../cmake/configure_binary.cmake)
add_slang_shader_target(multiviewport__main_sh OUT_BINARY_NAME "main" SOURCES "multiviewport/main.slang" ENTRY_POINTS mainVert mainFrag)
configure_binary(NAME multiviewport 
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os
    SHADER_TARGETS multiviewport__main_sh 
)
