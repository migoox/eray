include(../../cmake/add_slang_shader_target.cmake)
include(../../cmake/configure_binary.cmake)
add_slang_shader_target(compute_particles__main_sh OUT_BINARY_NAME "main" SOURCES "sandbox/main.slang" ENTRY_POINTS mainVert mainFrag)
add_slang_shader_target(compute_particles__particle_sh OUT_BINARY_NAME "particle" SOURCES "sandbox/particle.slang" ENTRY_POINTS mainComp)
configure_binary(NAME compute_particles
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os
    SHADER_TARGETS compute_particles__main_sh compute_particles__particle_sh
)
