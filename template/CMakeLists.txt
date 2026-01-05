include(../eray/cmake/add_slang_shader_target.cmake)
include(../eray/cmake/configure_binary.cmake)

add_slang_shader_target(__targetname____mesh_phong OUT_BINARY_NAME "mesh_phong" SOURCES "__targetname__/mesh_phong.slang" ENTRY_POINTS mainVert mainFrag)

configure_binary(
    NAME __targetname__
    DEPS_PUBLIC liberay-vkren liberay-res liberay-os liberay-math implot
    SHADER_TARGETS __targetname____mesh_phong
)
