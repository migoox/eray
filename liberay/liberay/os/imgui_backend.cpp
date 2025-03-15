#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <expected>
#include <liberay/os/imgui_backend.hpp>
#include <memory>

namespace eray::os {

std::expected<std::unique_ptr<ImGuiGLFWBackend>, ImGuiGLFWBackend::ImGuiBackendCreationError> ImGuiGLFWBackend::create(
    Driver driver) {
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;

  ImGui::StyleColorsDark();

  if (driver != Driver::OpenGL) {  // NOLINT
    return std::unexpected(ImGuiBackendCreationError::DriverNotSupported);
  }

  return std::unique_ptr<ImGuiGLFWBackend>(new ImGuiGLFWBackend(driver));
}

void ImGuiGLFWBackend::init_driver(void* window) {
  const char* glsl_version = "#version 130";

  if (driver_ == Driver::OpenGL) {
    ImGui_ImplGlfw_InitForOpenGL(reinterpret_cast<GLFWwindow*>(window), true);
    ImGui_ImplOpenGL3_Init(glsl_version);
  } else if (driver_ == Driver::Vulcan) {  // NOLINT
    // TODO(migoox): add vulcan integration
    // ImGui_ImplGlfw_InitForVulkan(reinterpret_cast<GLFWwindow*>(window), true);
    // ImGui_ImplVulcan_Init(glsl_version);
  }
}

ImGuiGLFWBackend::ImGuiGLFWBackend(Driver driver) : driver_(driver) {}

void ImGuiGLFWBackend::new_frame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImGuiGLFWBackend::generate_draw_data() { ImGui::Render(); }

void ImGuiGLFWBackend::render_draw_data() {
  if (driver_ == Driver::OpenGL) {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  } else if (driver_ == Driver::Vulcan) {
    // TODO(migoox): add vulcan integration
  }
}

ImGuiGLFWBackend::~ImGuiGLFWBackend() {
  if (driver_ == Driver::OpenGL) {
    ImGui_ImplOpenGL3_Shutdown();
  } else if (driver_ == Driver::Vulcan) {
    // TODO(migoox): add vulcan integration
  }
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

}  // namespace eray::os
