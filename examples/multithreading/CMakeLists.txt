include(../../cmake/add_slang_shader_target.cmake)
include(../../cmake/configure_binary.cmake)
add_slang_shader_target(multithreading__main_sh OUT_BINARY_NAME "main" SOURCES "multithreading/main.slang" ENTRY_POINTS mainVert mainFrag)
add_slang_shader_target(multithreading__particle_sh OUT_BINARY_NAME "particle" SOURCES "multithreading/particle.slang" ENTRY_POINTS mainComp)
configure_binary(NAME multithreading
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os
    SHADER_TARGETS multithreading__main_sh multithreading__particle_sh
)
