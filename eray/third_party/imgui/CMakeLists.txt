include(../../../cmake/CPM.cmake)

CPMAddPackage(
  NAME imgui 
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.91.6-docking
  SYSTEM YES
)

add_library(
  imgui
  STATIC 
  GLOBAL
)

target_sources(
  imgui

  PRIVATE
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_rectpack.h
  ${imgui_SOURCE_DIR}/imgui_textedit.h
  ${imgui_SOURCE_DIR}/imgui_truetype.h

  PUBLIC
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.h
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.h
  ${imgui_SOURCE_DIR}/imgui.h
  ${imgui_SOURCE_DIR}/imgui_internal.h
)

set_property(TARGET imgui PROPERTY COMPILE_WARNING_AS_ERROR OFF)
