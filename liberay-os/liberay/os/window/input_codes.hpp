#pragma once

#include <cstdint>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/zstring_view.hpp>.hpp>

namespace eray::os {

enum class KeyCode : uint8_t {
  // Letters
  A = 0,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  I,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,

  // Numbers
  Num0,
  Num1,
  Num2,
  Num3,
  Num4,
  Num5,
  Num6,
  Num7,
  Num8,
  Num9,

  // Function s
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,

  // Arrow s
  ArrowUp,
  ArrowDown,
  ArrowLeft,
  ArrowRight,

  // Special s
  Escape,
  Enter,
  Backspace,
  Tab,
  Space,
  LeftShift,
  RightShift,
  LeftControl,
  RightControl,
  LeftAlt,
  RightAlt,

  // Additional s
  Insert,
  Delete,
  Home,
  End,
  PageUp,
  PageDown,

  // Punctuation and symbols
  Apostrophe,
  Comma,
  Minus,
  Period,
  Slash,
  Semicolon,
  Equal,
  LeftBracket,
  RightBracket,
  Backslash,
  GraveAccent,

  _Count,  // NOLINT
};

constexpr util::StringEnumMapper<KeyCode> kKeyCodeNames({
    // Letters
    {KeyCode::A, "A"},
    {KeyCode::B, "B"},
    {KeyCode::C, "C"},
    {KeyCode::D, "D"},
    {KeyCode::E, "E"},
    {KeyCode::F, "F"},
    {KeyCode::G, "G"},
    {KeyCode::H, "H"},
    {KeyCode::I, "I"},
    {KeyCode::J, "J"},
    {KeyCode::K, "K"},
    {KeyCode::L, "L"},
    {KeyCode::M, "M"},
    {KeyCode::N, "N"},
    {KeyCode::O, "O"},
    {KeyCode::P, "P"},
    {KeyCode::Q, "Q"},
    {KeyCode::R, "R"},
    {KeyCode::S, "S"},
    {KeyCode::T, "T"},
    {KeyCode::U, "U"},
    {KeyCode::V, "V"},
    {KeyCode::W, "W"},
    {KeyCode::X, "X"},
    {KeyCode::Y, "Y"},
    {KeyCode::Z, "Z"},

    // Numbers
    {KeyCode::Num0, "Num0"},
    {KeyCode::Num1, "Num1"},
    {KeyCode::Num2, "Num2"},
    {KeyCode::Num3, "Num3"},
    {KeyCode::Num4, "Num4"},
    {KeyCode::Num5, "Num5"},
    {KeyCode::Num6, "Num6"},
    {KeyCode::Num7, "Num7"},
    {KeyCode::Num8, "Num8"},
    {KeyCode::Num9, "Num9"},

    // Function s
    {KeyCode::F1, "F1"},
    {KeyCode::F2, "F2"},
    {KeyCode::F3, "F3"},
    {KeyCode::F4, "F4"},
    {KeyCode::F5, "F5"},
    {KeyCode::F6, "F6"},
    {KeyCode::F7, "F7"},
    {KeyCode::F8, "F8"},
    {KeyCode::F9, "F9"},
    {KeyCode::F10, "F10"},
    {KeyCode::F11, "F11"},
    {KeyCode::F12, "F12"},

    // Arrow s
    {KeyCode::ArrowUp, "ArrowUp"},
    {KeyCode::ArrowDown, "ArrowDown"},
    {KeyCode::ArrowLeft, "ArrowLeft"},
    {KeyCode::ArrowRight, "ArrowRight"},

    // Special s
    {KeyCode::Escape, "Escape"},
    {KeyCode::Enter, "Enter"},
    {KeyCode::Backspace, "Backspace"},
    {KeyCode::Tab, "Tab"},
    {KeyCode::Space, "Space"},
    {KeyCode::LeftShift, "LeftShift"},
    {KeyCode::RightShift, "RightShift"},
    {KeyCode::LeftControl, "LeftControl"},
    {KeyCode::RightControl, "RightControl"},
    {KeyCode::LeftAlt, "LeftAlt"},
    {KeyCode::RightAlt, "RightAlt"},

    // Additional s
    {KeyCode::Insert, "Insert"},
    {KeyCode::Delete, "Delete"},
    {KeyCode::Home, "Home"},
    {KeyCode::End, "End"},
    {KeyCode::PageUp, "PageUp"},
    {KeyCode::PageDown, "PageDown"},

    // Punctuation and symbols
    {KeyCode::Apostrophe, "Apostrophe"},
    {KeyCode::Comma, "Comma"},
    {KeyCode::Minus, "Minus"},
    {KeyCode::Period, "Period"},
    {KeyCode::Slash, "Slash"},
    {KeyCode::Semicolon, "Semicolon"},
    {KeyCode::Equal, "Equal"},
    {KeyCode::LeftBracket, "LeftBracket"},
    {KeyCode::RightBracket, "RightBracket"},
    {KeyCode::Backslash, "Backslash"},
    {KeyCode::GraveAccent, "GraveAccent"},
});

constexpr util::zstring_view key_code_name(KeyCode code) { return kKeyCodeNames[code]; }

enum class MouseBtnCode : uint8_t {
  MouseButtonLeft   = 0,
  MouseButtonRight  = 1,
  MouseButtonMiddle = 2,
  MouseButton4      = 3,
  MouseButton5      = 4,
  MouseButton6      = 5,
  MouseButton7      = 6,
  MouseButton8      = 7,

  _Count = 8,  // NOLINT

  MouseButton1 = MouseButtonLeft,
  MouseButton2 = MouseButtonRight,
  MouseButton3 = MouseButtonMiddle
};

constexpr util::StringEnumMapper<MouseBtnCode> kMouseCodeNames({
    {MouseBtnCode::MouseButtonLeft, "MouseButtonLeft"},      //
    {MouseBtnCode::MouseButtonRight, "MouseButtonRight"},    //
    {MouseBtnCode::MouseButtonMiddle, "MouseButtonMiddle"},  //
    {MouseBtnCode::MouseButton4, "MouseButton4"},            //
    {MouseBtnCode::MouseButton5, "MouseButton5"},            //
    {MouseBtnCode::MouseButton6, "MouseButton6"},            //
    {MouseBtnCode::MouseButton7, "MouseButton7"},            //
    {MouseBtnCode::MouseButton8, "MouseButton8"}             //
});

constexpr util::zstring_view mouse_code_name(MouseBtnCode code) { return kMouseCodeNames[code]; }

}  // namespace eray::os
