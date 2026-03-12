include(CPM)

CPMAddPackage(
  NAME imgui 
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.91.6-docking
  SYSTEM YES
  DOWNLOAD_ONLY YES
)

add_library(imgui STATIC)

target_link_libraries(imgui
    PUBLIC
    third_party::glfw
    third_party::vulkan
)

target_sources(imgui

  PRIVATE
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imstb_rectpack.h
  ${imgui_SOURCE_DIR}/imstb_textedit.h
  ${imgui_SOURCE_DIR}/imstb_truetype.h

  PUBLIC
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.h
  ${imgui_SOURCE_DIR}/imgui.h
  ${imgui_SOURCE_DIR}/imgui_internal.h
)

if(ERAY_BUILD_VULKAN_RHI)
  target_sources(imgui
      PRIVATE
      ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp

      PUBLIC
      ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.h
  )
endif()

if(ERAY_BUILD_DX12_RHI)
  target_sources(imgui
      PRIVATE
      ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp

      PUBLIC
      ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.h
  )
endif()

target_include_directories(imgui
    PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

set_target_properties(imgui PROPERTIES COMPILE_WARNING_AS_ERROR OFF)

add_library(third_party::imgui ALIAS imgui)