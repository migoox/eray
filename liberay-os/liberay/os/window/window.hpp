#pragma once

#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/driver.hpp>
#include <liberay/os/imgui_backend.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/input_codes.hpp>
#include <liberay/os/window/mouse_cursor_codes.hpp>
#include <liberay/os/window/window_props.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/zstring_view.hpp>

namespace eray::os {

/**
 * @brief An interface that represents an application window. It provides abstraction over multiple window
 * backends e.g. GLFW and WinAPI.
 *
 */
class Window {
 public:
  Window()          = delete;
  virtual ~Window() = default;

  explicit Window(WindowProperties&& props) : props_(std::move(props)) {
    event_dispatcher_.set_event_callback<WindowResizedEvent>(
        class_method_as_event_callback(this, &Window::on_win_resized));
    event_dispatcher_.set_event_callback<WindowMovedEvent>(class_method_as_event_callback(this, &Window::on_win_moved));
  }

  virtual ImGuiBackend& imgui() = 0;

  util::zstring_view title() const { return props_.title; }
  virtual void set_title(util::zstring_view title) = 0;

  math::Vec2i pos() const { return math::Vec2i(props_.pos.x, props_.pos.y); }
  virtual void set_pos(math::Vec2i pos) = 0;

  math::Vec2i size() const { return props_.size; }
  virtual void set_size(math::Vec2i size) = 0;

  bool vsync() const { return props_.vsync; }
  virtual void set_vsync(bool vsync) = 0;

  bool fullscreen() const { return props_.fullscreen; }
  virtual void set_fullscreen(bool fullscreen) = 0;

  virtual math::Vec2d mouse_pos() const     = 0;
  virtual math::Vec2d mouse_pos_ndc() const = 0;

  virtual Driver driver() const = 0;

  /**
   * @brief Polls events and swaps the back buffer. Invoked by the application main loop (the `run` function).
   *
   * @tparam TEvent
   * @param callback_fn
   */
  virtual void update() = 0;

  virtual bool should_close() const = 0;

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
  virtual CursorMode get_mouse_cursor_mode()                 = 0;

 protected:
  bool on_win_moved(const WindowMovedEvent& ev) {
    props_.pos = math::Vec2i(ev.x(), ev.y());
    util::Logger::debug("moved ({}, {})", ev.x(), ev.y());
    return true;
  }

  bool on_win_resized(const WindowResizedEvent& ev) {
    props_.size = math::Vec2i(ev.width(), ev.height());
    util::Logger::debug("resized ({}, {})", ev.width(), ev.height());
    return true;
  }

 protected:
  WindowProperties props_;
  WindowEventDispatcher event_dispatcher_;
};

}  // namespace eray::os
