#pragma once

#include <liberay/os/imgui_backend.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/os/window/window_props.hpp>
#include <liberay/util/ruleof.hpp>
#include <memory>

namespace eray::os {

class GLFWWindowBackend;

class GLFWWindow final : public Window {
 public:
  GLFWWindow() = delete;
  ERAY_DELETE_COPY_AND_MOVE(GLFWWindow)

  ~GLFWWindow() final;

  ImGuiBackend& imgui() final { return *imgui_; }

  void update() final;

  void set_title(util::zstring_view title) final;

  void set_pos(math::Vec2i pos) final;

  void set_size(math::Vec2i size) final;

  void set_vsync(bool vsync) final;

  void set_fullscreen(bool fullscreen) final;

  math::Vec2d mouse_pos() const final;
  math::Vec2d mouse_pos_ndc() const final;

  Driver driver() const final { return driver_; }

  bool is_btn_held(KeyCode code) final;
  bool is_mouse_btn_held(MouseBtnCode code) final;

  void set_mouse_cursor_mode(CursorMode cursor_mode) final;
  CursorMode get_mouse_cursor_mode() final;

 private:
  friend GLFWWindowBackend;
  explicit GLFWWindow(void* glfw_window_ptr, WindowProperties props, Driver driver,
                      std::unique_ptr<ImGuiBackend> imgui);

  void init_dispatcher();

 private:
  void* glfw_window_ptr_;
  Driver driver_;
  std::unique_ptr<ImGuiBackend> imgui_;
};

}  // namespace eray::os
