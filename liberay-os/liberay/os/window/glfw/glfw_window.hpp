#pragma once

#include <liberay/os/window/window.hpp>
#include <liberay/os/window/window_props.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/ruleof.hpp>

namespace eray::os {

class GLFWWindow final : public Window {
 public:
  GLFWWindow() = delete;
  GLFWWindow(void* glfw_window_ptr, const WindowProperties& props, WindowAPI window_api);
  ERAY_DELETE_COPY_AND_MOVE(GLFWWindow)
  ~GLFWWindow() final;

  void poll_events() final;

  void set_title(util::zstring_view title) final;
  void set_window_size(int width, int height) final;
  void set_fullscreen(bool fullscreen) final;

  Dimensions framebuffer_size() const final;
  MousePosition mouse_pos() const final;

  WindowAPI window_api() const final { return window_api_; }

  bool is_btn_held(KeyCode code) final;
  bool is_mouse_btn_held(MouseBtnCode code) final;

  void set_mouse_cursor_mode(CursorMode cursor_mode) final;
  CursorMode mouse_cursor_mode() final;

  bool should_close() const final;

  void* win_handle() const final { return glfw_window_ptr_; }

 private:
  void init_dispatcher();

 private:
  void* glfw_window_ptr_;
  WindowAPI window_api_;
};

}  // namespace eray::os
