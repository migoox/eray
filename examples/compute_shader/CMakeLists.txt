include(../../cmake/add_slang_shader_target.cmake)
include(../../cmake/configure_binary.cmake)
add_slang_shader_target(compute_shader__main_sh OUT_BINARY_NAME "main" SOURCES "compute_shader/main.slang" ENTRY_POINTS mainVert mainFrag)
add_slang_shader_target(compute_shader__particle_sh OUT_BINARY_NAME "particle" SOURCES "compute_shader/particle.slang" ENTRY_POINTS mainComp)
configure_binary(NAME compute_shader
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os
    SHADER_TARGETS compute_shader__main_sh compute_shader__particle_sh
)
