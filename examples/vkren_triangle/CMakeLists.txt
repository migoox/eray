include(../../cmake/add_slang_shader_target.cmake)
include(../../cmake/configure_binary.cmake)
add_slang_shader_target(vkren_triangle__main_sh OUT_BINARY_NAME "main" SOURCES "vkren_triangle/main.slang" ENTRY_POINTS mainVert mainFrag)
configure_binary(NAME vkren_triangle
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os
    SHADER_TARGETS vkren_triangle__main_sh
)
