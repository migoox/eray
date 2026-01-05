#pragma once

#include <concepts>
#include <liberay/os/window/input_codes.hpp>
#include <liberay/os/window/mouse_cursor_codes.hpp>
#include <liberay/os/window/window.hpp>
#include <memory>
#include <unordered_set>

namespace eray::os {

class InputManager {
 public:
  static std::unique_ptr<InputManager> create(std::shared_ptr<Window> window);

  InputManager()                             = delete;
  InputManager(InputManager&&) noexcept      = default;
  explicit InputManager(const InputManager&) = delete;

  InputManager& operator=(InputManager&&) noexcept = default;
  InputManager& operator=(const InputManager&)     = delete;

  /**
   * @brief Returns true when the user has started pressing the key in the current physics tick. It
   * will only return true on tick that the user pressed down the button.
   */
  bool is_key_just_pressed(KeyCode key_code) const;

  /**
   * @brief Returns true when the user has stopped pressing the key in the current physics tick. It
   * will only return true on tick that the user pressed down the button.
   */
  bool is_key_just_released(KeyCode key_code) const;

  /**
   * @brief Returns true when the keyboard button is down.
   */
  bool is_key_pressed(KeyCode key_code) const;

  bool is_anything_pressed() const;

  /**
   * @brief Returns true when the user has started pressing the mouse button in the current physics tick. It
   * will only return true on tick that the user pressed down the mouse button.
   */
  bool is_mouse_btn_just_pressed(MouseBtnCode mouse_btn_code) const;

  /**
   * @brief Returns true when the user has stopped pressing the mouse button in the current physics tick. It
   * will only return true on tick that the user pressed down the mouse button.
   */
  bool is_mouse_btn_just_released(MouseBtnCode mouse_btn_code) const;

  bool just_scrolled() const { return just_scrolled_; }

  /**
   * @brief Returns true when the button is down.
   */
  bool is_mouse_btn_pressed(MouseBtnCode mouse_btn_code) const;

  template <typename T = float>
    requires std::floating_point<T>
  T mouse_pos_x() const {
    if constexpr (std::is_same_v<T, double>) {
      return mouse_pos_x_;
    }
    return static_cast<float>(mouse_pos_x_);
  }

  template <typename T = float>
    requires std::floating_point<T>
  T mouse_pos_y() const {
    if constexpr (std::is_same_v<T, double>) {
      return mouse_pos_y_;
    }
    return static_cast<float>(mouse_pos_y_);
  }

  template <typename T = float>
    requires std::floating_point<T>
  T last_mouse_pos_x() const {
    if constexpr (std::is_same_v<T, double>) {
      return last_mouse_pos_x_;
    }
    return static_cast<float>(last_mouse_pos_x_);
  }

  template <typename T = float>
    requires std::floating_point<T>
  T last_mouse_pos_y() const {
    if constexpr (std::is_same_v<T, double>) {
      return last_mouse_pos_y_;
    }
    return static_cast<float>(last_mouse_pos_y_);
  }

  template <typename T = float>
    requires std::floating_point<T>
  T delta_mouse_pos_x() const {
    if constexpr (std::is_same_v<T, double>) {
      return mouse_pos_x_ - last_mouse_pos_x_;
    }
    return static_cast<float>(mouse_pos_x_ - last_mouse_pos_x_);
  }

  template <typename T = float>
    requires std::floating_point<T>
  T delta_mouse_pos_y() const {
    if constexpr (std::is_same_v<T, double>) {
      return mouse_pos_y_ - last_mouse_pos_y_;
    }
    return static_cast<float>(mouse_pos_y_ - last_mouse_pos_y_);
  }

  template <typename T = float>
    requires std::floating_point<T>
  T delta_mouse_scroll_x() const {
    if constexpr (std::is_same_v<T, double>) {
      return mouse_scroll_x_;
    }
    return static_cast<float>(mouse_scroll_x_);
  }

  template <typename T = float>
    requires std::floating_point<T>
  T delta_mouse_scroll_y() const {
    if constexpr (std::is_same_v<T, double>) {
      return mouse_scroll_y_;
    }
    return static_cast<float>(mouse_scroll_y_);
  }

  bool is_mouse_on_window() const { return is_mouse_on_window_; }

  void set_mouse_cursor_mode(CursorMode cursor_mode);
  CursorMode cursor_mode() const;

  bool is_input_captured() const { return is_input_captured_; }

  /**
   * @brief Called automatically bo the application.
   */
  void process();

  /**
   * @brief Called automatically bo the application.
   */
  void prepare(bool input_captured) {
    mouse_pos_x_ = window_->mouse_pos().x;
    mouse_pos_y_ = window_->mouse_pos().y;

    is_input_captured_ = input_captured;
  }

 private:
  explicit InputManager(std::shared_ptr<Window> window);

 private:
  std::array<bool, static_cast<uint8_t>(KeyCode::_Count)> is_key_pressed_{};
  std::unordered_set<uint8_t> keys_just_pressed_;
  std::unordered_set<uint8_t> keys_just_released_;

  std::array<bool, static_cast<uint8_t>(MouseBtnCode::_Count)> is_mouse_btn_pressed_{};
  std::unordered_set<uint8_t> mouse_btns_just_pressed_;
  std::unordered_set<uint8_t> mouse_btns_just_released_;
  bool just_scrolled_;

  double last_mouse_pos_x_ = 0.;
  double last_mouse_pos_y_ = 0.;

  double mouse_pos_x_ = 0.;
  double mouse_pos_y_ = 0.;

  double mouse_scroll_x_ = 0.;
  double mouse_scroll_y_ = 0.;

  int pressed_count_ = 0;

  bool is_mouse_on_window_ = true;
  bool is_input_captured_  = false;

  std::shared_ptr<Window> window_;
};

}  // namespace eray::os
