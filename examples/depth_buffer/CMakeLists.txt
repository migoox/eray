include(../../cmake/add_slang_shader_target.cmake)
include(../../cmake/configure_binary.cmake)
add_slang_shader_target(depth_buffer__main_sh OUT_BINARY_NAME "main" SOURCES "depth_buffer/main.slang" ENTRY_POINTS mainVert mainFrag)
configure_binary(NAME depth_buffer
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os
    SHADER_TARGETS depth_buffer__main_sh
)
