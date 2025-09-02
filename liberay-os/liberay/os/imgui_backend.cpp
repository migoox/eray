#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <expected>
#include <liberay/os/imgui_backend.hpp>
#include <liberay/os/system.hpp>
#include <liberay/util/logger.hpp>
#include <memory>

namespace eray::os {

std::expected<std::unique_ptr<ImGuiGLFWBackend>, ImGuiGLFWBackend::ImGuiBackendCreationError> ImGuiGLFWBackend::create(
    RenderingAPI driver) {
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;
  auto path      = System::path_to_utf8str(System::executable_dir() / "imgui.ini");
  ImGui::LoadIniSettingsFromDisk(path.c_str());

  ImGui::StyleColorsDark();

  if (driver != RenderingAPI::OpenGL) {  // NOLINT
    return std::unexpected(ImGuiBackendCreationError::DriverNotSupported);
  }

  return std::unique_ptr<ImGuiGLFWBackend>(new ImGuiGLFWBackend(driver));
}

void ImGuiGLFWBackend::init_driver(void* window) {
  const char* glsl_version = "#version 130";

  if (driver_ == RenderingAPI::OpenGL) {
    ImGui_ImplGlfw_InitForOpenGL(reinterpret_cast<GLFWwindow*>(window), true);
    ImGui_ImplOpenGL3_Init(glsl_version);
  } else if (driver_ == RenderingAPI::Vulcan) {  // NOLINT
    // TODO(migoox): add vulcan integration
    // ImGui_ImplGlfw_InitForVulkan(reinterpret_cast<GLFWwindow*>(window), true);
    // ImGui_ImplVulcan_Init(glsl_version);
  }
}

ImGuiGLFWBackend::ImGuiGLFWBackend(RenderingAPI driver) : driver_(driver) {}

void ImGuiGLFWBackend::new_frame() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImGuiGLFWBackend::generate_draw_data() { ImGui::Render(); }

void ImGuiGLFWBackend::render_draw_data() {
  if (driver_ == RenderingAPI::OpenGL) {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  } else if (driver_ == RenderingAPI::Vulcan) {
    // TODO(migoox): add vulcan integration
  }
}

ImGuiGLFWBackend::~ImGuiGLFWBackend() {
  util::Logger::info("Saved imgui.ini file");
  auto path = System::path_to_utf8str(System::executable_dir() / "imgui.ini");
  ImGui::SaveIniSettingsToDisk(path.c_str());
  if (driver_ == RenderingAPI::OpenGL) {
    ImGui_ImplOpenGL3_Shutdown();
  } else if (driver_ == RenderingAPI::Vulcan) {
    // TODO(migoox): add vulcan integration
  }
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

}  // namespace eray::os
