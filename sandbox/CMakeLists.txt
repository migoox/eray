include(../cmake/add_slang_shader_target.cmake)
include(../cmake/configure_binary.cmake)
add_slang_shader_target(sandbox__main_sh OUT_BINARY_NAME "main" SOURCES "sandbox/main.slang" ENTRY_POINTS mainVert mainFrag)
add_slang_shader_target(sandbox__particle_sh OUT_BINARY_NAME "particle" SOURCES "sandbox/particle.slang" ENTRY_POINTS mainComp)
configure_binary(NAME sandbox
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os
    SHADER_TARGETS sandbox__main_sh sandbox__particle_sh
)
