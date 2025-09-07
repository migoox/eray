#pragma once

#include <liberay/os/rendering_api.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/input_codes.hpp>
#include <liberay/os/window/mouse_cursor_codes.hpp>
#include <liberay/os/window/window_props.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/zstring_view.hpp>

namespace eray::os {

/**
 * @brief Abstract class that represents an application window. It provides abstraction over multiple window
 * backends e.g. GLFW and WinAPI. Window is RenderingAPI agnostic. To write window-renderer integration use
 * `win_handle()` combined with `window_api()`.
 *
 */
class Window {
 public:
  Window()          = delete;
  virtual ~Window() = default;

  struct MousePosition {
    double x;
    double y;
  };

  struct Dimensions {
    uint32_t width;
    uint32_t height;
  };

  explicit Window(const WindowProperties& props) : props_(std::move(props)) {
    event_dispatcher_.set_event_callback<WindowResizedEvent>(
        class_method_as_event_callback(this, &Window::on_win_resized));
  }

  util::zstring_view title() const { return props_.title; }
  virtual void set_title(util::zstring_view title) = 0;

  /**
   * @brief Returns screen-space window size. To get the framebuffer size, se `framebuffer_size()` function.
   *
   * @return Dimensions
   */
  Dimensions window_size() const { return {.width = props_.width, .height = props_.height}; }
  virtual void set_window_size(int width, int height) = 0;

  bool fullscreen() const { return props_.fullscreen; }
  virtual void set_fullscreen(bool fullscreen) = 0;

  /**
   * @brief Returns current framebuffer size in pixels. Use this to set the size of your framebuffer instead of the
   * `window_size()` function. On high DPI displays (like Apple's Retina display), screen coordinates returned by
   * `window_size()` does not correspond to pixels of the framebuffer.
   *
   * @return FramebufferSize
   */
  virtual Dimensions framebuffer_size() const = 0;

  virtual MousePosition mouse_pos() const = 0;
  virtual WindowAPI window_api() const    = 0;

  /**
   * @brief Polls events.
   *
   * @tparam TEvent
   * @param callback_fn
   */
  virtual void poll_events() = 0;

  virtual bool should_close() const                 = 0;
  virtual bool is_btn_held(KeyCode code)            = 0;
  virtual bool is_mouse_btn_held(MouseBtnCode code) = 0;

  /**
   * @brief Subscribe to event notifications dispatched by window event dispatcher. It's
   * possible to set multiple event callbacks to one event. For each event type, the last added callback will be invoked
   * first.
   *
   * @tparam TEvent
   * @param callback_fn
   */
  template <CWindowEvent TEvent>
  void set_event_callback(const EventCallback<TEvent>& callback_fn) {
    event_dispatcher_.set_event_callback(callback_fn);
  }

  /**
   * @brief Flushes all generated events and processes them.
   *
   */
  void process_queued_events() { event_dispatcher_.process_queued_events(); }

  virtual void set_mouse_cursor_mode(CursorMode cursor_mode) = 0;
  virtual CursorMode mouse_cursor_mode()                     = 0;

  virtual void* win_handle() const = 0;

 protected:
  bool on_win_resized(const WindowResizedEvent& ev) {
    props_.width  = ev.width();
    props_.height = ev.height();
    return true;
  }

 protected:
  WindowProperties props_;
  WindowEventDispatcher event_dispatcher_;
};

}  // namespace eray::os
