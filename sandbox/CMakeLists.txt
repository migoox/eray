project(sandbox CXX)

configure_slang_shader(
    TARGET_NAME particle_shader 
    OUT_BINARY "shaders/out/particle.spv" 

    SHADER_SOURCES 
    "shaders/particle.slang"

    ENTRY_POINTS 
    "mainComp"
)

configure_executable(
    TARGET_NAME sandbox

    PUBLIC_LINK_DEPS 
    third_party::glfw 
    third_party::imgui
    third_party::nfd
    third_party::vulkan
    third_party::stb_image

    POST_BUILD_COPY_DIRS
    "assets"
    "shaders"
)
add_dependencies(sandbox particle_shader)