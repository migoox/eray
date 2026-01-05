#include <liberay/os/input.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/mouse_cursor_codes.hpp>
#include <memory>

namespace eray::os {

std::unique_ptr<InputManager> InputManager::create(std::shared_ptr<Window> window) {
  auto* input_manager = new InputManager(std::move(window));  // NOLINT

  input_manager->window_->set_event_callback<KeyPressedEvent>(
      [input_manager](const KeyPressedEvent& key_pressed_event) {
        auto key_code = static_cast<uint8_t>(key_pressed_event.key_code());

        input_manager->is_key_pressed_[key_code] = true;
        input_manager->keys_just_pressed_.insert(key_code);
        ++input_manager->pressed_count_;

        return false;
      });

  input_manager->window_->set_event_callback<KeyReleasedEvent>(
      [input_manager](const KeyReleasedEvent& key_pressed_event) {
        auto key_code = static_cast<uint8_t>(key_pressed_event.key_code());

        input_manager->is_key_pressed_[key_code] = false;
        input_manager->keys_just_released_.insert(key_code);
        input_manager->pressed_count_ = std::max(0, input_manager->pressed_count_ - 1);

        return false;
      });

  input_manager->window_->set_event_callback<MouseButtonPressedEvent>(
      [input_manager](const MouseButtonPressedEvent& mouse_btn_pressed_event) {
        auto key_code = static_cast<uint8_t>(mouse_btn_pressed_event.mouse_btn_code());

        input_manager->is_mouse_btn_pressed_[key_code] = true;
        input_manager->mouse_btns_just_pressed_.insert(key_code);
        ++input_manager->pressed_count_;

        return false;
      });

  input_manager->window_->set_event_callback<MouseButtonReleasedEvent>(
      [input_manager](const MouseButtonReleasedEvent& mouse_btn_pressed_event) {
        auto key_code = static_cast<uint8_t>(mouse_btn_pressed_event.mouse_btn_code());

        input_manager->is_mouse_btn_pressed_[key_code] = false;
        input_manager->mouse_btns_just_released_.insert(key_code);
        input_manager->pressed_count_ = std::max(0, input_manager->pressed_count_ - 1);

        return false;
      });

  input_manager->window_->set_event_callback<MouseScrolledEvent>(
      [input_manager](const MouseScrolledEvent& mouse_scrolled_event) {
        input_manager->mouse_scroll_x_ = mouse_scrolled_event.x_offset();
        input_manager->mouse_scroll_y_ = mouse_scrolled_event.y_offset();

        return false;
      });

  input_manager->window_->set_event_callback<MouseEntered>([input_manager](const auto&) {
    input_manager->is_mouse_on_window_ = true;

    return false;
  });

  input_manager->window_->set_event_callback<MouseLeft>([input_manager](const auto&) {
    input_manager->is_mouse_on_window_ = false;

    return false;
  });

  auto result = std::unique_ptr<InputManager>(input_manager);
  result->is_key_pressed_.fill(false);
  result->is_mouse_btn_pressed_.fill(false);

  return result;
}

bool InputManager::is_key_just_pressed(KeyCode key_code) const {
  auto code = static_cast<uint8_t>(key_code);
  return keys_just_pressed_.contains(code);
}

bool InputManager::is_key_just_released(KeyCode key_code) const {
  auto code = static_cast<uint8_t>(key_code);
  return keys_just_released_.contains(code);
}

bool InputManager::is_key_pressed(KeyCode key_code) const {
  auto code = static_cast<uint8_t>(key_code);
  return is_key_pressed_[code];
}

bool InputManager::is_anything_pressed() const { return pressed_count_ != 0; }

bool InputManager::is_mouse_btn_just_pressed(MouseBtnCode mouse_btn_code) const {
  auto code = static_cast<uint8_t>(mouse_btn_code);
  return mouse_btns_just_pressed_.contains(code);
}

bool InputManager::is_mouse_btn_just_released(MouseBtnCode mouse_btn_code) const {
  auto code = static_cast<uint8_t>(mouse_btn_code);
  return mouse_btns_just_released_.contains(code);
}

bool InputManager::is_mouse_btn_pressed(MouseBtnCode mouse_btn_code) const {
  auto code = static_cast<uint8_t>(mouse_btn_code);
  return is_mouse_btn_pressed_[code];
}

InputManager::InputManager(std::shared_ptr<Window> window) : window_(std::move(window)) {}

void InputManager::set_mouse_cursor_mode(CursorMode cursor_mode) {
  if (!window_->is_destroyed()) {
    window_->set_mouse_cursor_mode(cursor_mode);
  }
}

CursorMode InputManager::cursor_mode() const {
  if (!window_->is_destroyed()) {
    return window_->mouse_cursor_mode();
  }
  return CursorMode::Normal;
}

void InputManager::process() {
  keys_just_pressed_.clear();
  keys_just_released_.clear();
  mouse_btns_just_pressed_.clear();
  mouse_btns_just_released_.clear();

  last_mouse_pos_x_ = mouse_pos_x_;
  last_mouse_pos_y_ = mouse_pos_y_;

  mouse_scroll_x_ = 0.;
  mouse_scroll_y_ = 0.;
}

}  // namespace eray::os
