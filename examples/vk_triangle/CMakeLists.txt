include(../../cmake/add_slang_shader_target.cmake)
include(../../cmake/configure_binary.cmake)
add_slang_shader_target(vk_triangle__main_sh OUT_BINARY_NAME "main" SOURCES "vk_triangle/main.slang" ENTRY_POINTS mainVert mainFrag)
configure_binary(NAME vk_triangle 
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os
    SHADER_TARGETS vk_triangle__main_sh
)
