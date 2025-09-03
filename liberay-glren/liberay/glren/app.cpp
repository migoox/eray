#include <glad/gl.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <liberay/glren/app.hpp>
#include <liberay/glren/vertex_array.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>

namespace eray::glren {

Application::Application(std::unique_ptr<os::Window> window) : window_(std::move(window)) {
  window_->set_event_callback<os::WindowClosedEvent>(class_method_as_event_callback(this, &Application::on_closed));
}

void Application::run() {
  swap_chain_ = GLFWSwapChain::create(*window_);

  // == ImGui Integration ==============================================================================================
  const char* glsl_version = "#version 130";
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;
  auto path      = os::System::path_to_utf8str(os::System::executable_dir() / "imgui.ini");
  ImGui::LoadIniSettingsFromDisk(path.c_str());
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(swap_chain_.win_handle(), true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // == ImGui Integration ==============================================================================================
  Duration lag(0ns);
  Duration second(0ns);
  auto previous_time = Clock::now();

  uint16_t frames = 0U;
  uint16_t ticks  = 0U;

  while (running_) {
    auto current_time = Clock::now();
    auto delta        = current_time - previous_time;
    previous_time     = current_time;

    lag += std::chrono::duration_cast<Duration>(delta);
    second += std::chrono::duration_cast<Duration>(delta);

    window_->process_queued_events();

    while (lag >= kTickTime) {
      update(kTickTime);

      lag -= kTickTime;
      time_ += kTickTime;
      ++ticks;
    }

    ++frames;
    if (!minimized_) {
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      render_gui(delta);
      render(delta);
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    window_->poll_events();
    swap_chain_.swap_buffers();
    if (auto result = os::System::file_dialog().update(); !result) {
      util::Logger::err("File dialog update failed");
    }

    if (second > 1s) {
      uint16_t seconds = static_cast<uint16_t>(std::chrono::duration_cast<std::chrono::seconds>(second).count());

      fps_   = static_cast<uint16_t>(frames / seconds);
      tps_   = static_cast<uint16_t>(ticks / seconds);
      frames = 0;
      ticks  = 0;
      second = 0ns;
    }

    running_ = running_ && !window_->should_close();
  }
}

void Application::render_gui(Duration /* delta */) {}

void Application::render(Duration /* delta */) {
  ImGui::ShowDemoWindow();
  ERAY_GL_CALL(glClearColor(0.5F, 0.6F, 0.6F, 1.0F));
  ERAY_GL_CALL(glClear(GL_COLOR_BUFFER_BIT));
}

void Application::update(Duration /* delta */) {}

bool Application::on_closed(const os::WindowClosedEvent&) {
  running_ = false;
  return true;
}

Application::~Application() {
  util::Logger::info("Saved imgui.ini file");
  auto path = os::System::path_to_utf8str(os::System::executable_dir() / "imgui.ini");
  ImGui::SaveIniSettingsToDisk(path.c_str());
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

}  // namespace eray::glren
