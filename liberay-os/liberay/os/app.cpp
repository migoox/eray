#include <glad/gl.h>
#include <imgui/imgui.h>

#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/os/app.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/util/logger.hpp>

namespace eray::os {

Application::Application(std::unique_ptr<os::Window> window) : window_(std::move(window)) {
  window_->set_event_callback<WindowClosedEvent>(class_method_as_event_callback(this, &Application::on_closed));
}

void Application::run() {
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
      window_->imgui().new_frame();
      render_gui(delta);
      render(delta);
      window_->imgui().generate_draw_data();
      window_->imgui().render_draw_data();
    }

    window_->update();
    if (auto result = System::file_dialog().update(); !result) {
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

bool Application::on_closed(const WindowClosedEvent&) {
  running_ = false;
  return true;
}

}  // namespace eray::os
