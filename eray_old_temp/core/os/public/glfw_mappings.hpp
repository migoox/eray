#pragma once
#include <GLFW/glfw3.h>

#include <liberay/os/window/input_codes.hpp>
#include <liberay/os/window/mouse_cursor_codes.hpp>

namespace eray::os {

constexpr util::EnumMapper<KeyCode, int> kKeyCodeGLFWMapping({
    // Letters
    {KeyCode::A, GLFW_KEY_A},
    {KeyCode::B, GLFW_KEY_B},
    {KeyCode::C, GLFW_KEY_C},
    {KeyCode::D, GLFW_KEY_D},
    {KeyCode::E, GLFW_KEY_E},
    {KeyCode::F, GLFW_KEY_F},
    {KeyCode::G, GLFW_KEY_G},
    {KeyCode::H, GLFW_KEY_H},
    {KeyCode::I, GLFW_KEY_I},
    {KeyCode::J, GLFW_KEY_J},
    {KeyCode::K, GLFW_KEY_K},
    {KeyCode::L, GLFW_KEY_L},
    {KeyCode::M, GLFW_KEY_M},
    {KeyCode::N, GLFW_KEY_N},
    {KeyCode::O, GLFW_KEY_O},
    {KeyCode::P, GLFW_KEY_P},
    {KeyCode::Q, GLFW_KEY_Q},
    {KeyCode::R, GLFW_KEY_R},
    {KeyCode::S, GLFW_KEY_S},
    {KeyCode::T, GLFW_KEY_T},
    {KeyCode::U, GLFW_KEY_U},
    {KeyCode::V, GLFW_KEY_V},
    {KeyCode::W, GLFW_KEY_W},
    {KeyCode::X, GLFW_KEY_X},
    {KeyCode::Y, GLFW_KEY_Y},
    {KeyCode::Z, GLFW_KEY_Z},

    // Numbers
    {KeyCode::Num0, GLFW_KEY_0},
    {KeyCode::Num1, GLFW_KEY_1},
    {KeyCode::Num2, GLFW_KEY_2},
    {KeyCode::Num3, GLFW_KEY_3},
    {KeyCode::Num4, GLFW_KEY_4},
    {KeyCode::Num5, GLFW_KEY_5},
    {KeyCode::Num6, GLFW_KEY_6},
    {KeyCode::Num7, GLFW_KEY_7},
    {KeyCode::Num8, GLFW_KEY_8},
    {KeyCode::Num9, GLFW_KEY_9},

    // Function s
    {KeyCode::F1, GLFW_KEY_F1},
    {KeyCode::F2, GLFW_KEY_F2},
    {KeyCode::F3, GLFW_KEY_F3},
    {KeyCode::F4, GLFW_KEY_F4},
    {KeyCode::F5, GLFW_KEY_F5},
    {KeyCode::F6, GLFW_KEY_F6},
    {KeyCode::F7, GLFW_KEY_F7},
    {KeyCode::F8, GLFW_KEY_F8},
    {KeyCode::F9, GLFW_KEY_F9},
    {KeyCode::F10, GLFW_KEY_F10},
    {KeyCode::F11, GLFW_KEY_F11},
    {KeyCode::F12, GLFW_KEY_F12},

    // Arrow s
    {KeyCode::ArrowUp, GLFW_KEY_UP},
    {KeyCode::ArrowDown, GLFW_KEY_DOWN},
    {KeyCode::ArrowLeft, GLFW_KEY_LEFT},
    {KeyCode::ArrowRight, GLFW_KEY_RIGHT},

    // Special s
    {KeyCode::Escape, GLFW_KEY_ESCAPE},
    {KeyCode::Enter, GLFW_KEY_ENTER},
    {KeyCode::Backspace, GLFW_KEY_BACKSPACE},
    {KeyCode::Tab, GLFW_KEY_TAB},
    {KeyCode::Space, GLFW_KEY_SPACE},
    {KeyCode::LeftShift, GLFW_KEY_LEFT_SHIFT},
    {KeyCode::RightShift, GLFW_KEY_RIGHT_SHIFT},
    {KeyCode::LeftControl, GLFW_KEY_LEFT_CONTROL},
    {KeyCode::RightControl, GLFW_KEY_RIGHT_CONTROL},
    {KeyCode::LeftAlt, GLFW_KEY_LEFT_ALT},
    {KeyCode::RightAlt, GLFW_KEY_RIGHT_ALT},

    // Additional s
    {KeyCode::Insert, GLFW_KEY_INSERT},
    {KeyCode::Delete, GLFW_KEY_DELETE},
    {KeyCode::Home, GLFW_KEY_HOME},
    {KeyCode::End, GLFW_KEY_END},
    {KeyCode::PageUp, GLFW_KEY_PAGE_UP},
    {KeyCode::PageDown, GLFW_KEY_PAGE_DOWN},

    // Punctuation and symbols
    {KeyCode::Apostrophe, GLFW_KEY_APOSTROPHE},
    {KeyCode::Comma, GLFW_KEY_COMMA},
    {KeyCode::Minus, GLFW_KEY_MINUS},
    {KeyCode::Period, GLFW_KEY_PERIOD},
    {KeyCode::Slash, GLFW_KEY_SLASH},
    {KeyCode::Semicolon, GLFW_KEY_SEMICOLON},
    {KeyCode::Equal, GLFW_KEY_EQUAL},
    {KeyCode::LeftBracket, GLFW_KEY_LEFT_BRACKET},
    {KeyCode::RightBracket, GLFW_KEY_RIGHT_BRACKET},
    {KeyCode::Backslash, GLFW_KEY_BACKSLASH},
    {KeyCode::GraveAccent, GLFW_KEY_GRAVE_ACCENT},
});

inline int key_code_to_glfw(KeyCode code) { return kKeyCodeGLFWMapping[code]; }

// TODO(migoox): do it better
inline std::optional<KeyCode> key_code_from_glfw(int key) {
  switch (key) {
    // Letters
    case GLFW_KEY_A:
      return KeyCode::A;
    case GLFW_KEY_B:
      return KeyCode::B;
    case GLFW_KEY_C:
      return KeyCode::C;
    case GLFW_KEY_D:
      return KeyCode::D;
    case GLFW_KEY_E:
      return KeyCode::E;
    case GLFW_KEY_F:
      return KeyCode::F;
    case GLFW_KEY_G:
      return KeyCode::G;
    case GLFW_KEY_H:
      return KeyCode::H;
    case GLFW_KEY_I:
      return KeyCode::I;
    case GLFW_KEY_J:
      return KeyCode::J;
    case GLFW_KEY_K:
      return KeyCode::K;
    case GLFW_KEY_L:
      return KeyCode::L;
    case GLFW_KEY_M:
      return KeyCode::M;
    case GLFW_KEY_N:
      return KeyCode::N;
    case GLFW_KEY_O:
      return KeyCode::O;
    case GLFW_KEY_P:
      return KeyCode::P;
    case GLFW_KEY_Q:
      return KeyCode::Q;
    case GLFW_KEY_R:
      return KeyCode::R;
    case GLFW_KEY_S:
      return KeyCode::S;
    case GLFW_KEY_T:
      return KeyCode::T;
    case GLFW_KEY_U:
      return KeyCode::U;
    case GLFW_KEY_V:
      return KeyCode::V;
    case GLFW_KEY_W:
      return KeyCode::W;
    case GLFW_KEY_X:
      return KeyCode::X;
    case GLFW_KEY_Y:
      return KeyCode::Y;
    case GLFW_KEY_Z:
      return KeyCode::Z;

    // Numbers
    case GLFW_KEY_0:
      return KeyCode::Num0;
    case GLFW_KEY_1:
      return KeyCode::Num1;
    case GLFW_KEY_2:
      return KeyCode::Num2;
    case GLFW_KEY_3:
      return KeyCode::Num3;
    case GLFW_KEY_4:
      return KeyCode::Num4;
    case GLFW_KEY_5:
      return KeyCode::Num5;
    case GLFW_KEY_6:
      return KeyCode::Num6;
    case GLFW_KEY_7:
      return KeyCode::Num7;
    case GLFW_KEY_8:
      return KeyCode::Num8;
    case GLFW_KEY_9:
      return KeyCode::Num9;

    // Function s
    case GLFW_KEY_F1:
      return KeyCode::F1;
    case GLFW_KEY_F2:
      return KeyCode::F2;
    case GLFW_KEY_F3:
      return KeyCode::F3;
    case GLFW_KEY_F4:
      return KeyCode::F4;
    case GLFW_KEY_F5:
      return KeyCode::F5;
    case GLFW_KEY_F6:
      return KeyCode::F6;
    case GLFW_KEY_F7:
      return KeyCode::F7;
    case GLFW_KEY_F8:
      return KeyCode::F8;
    case GLFW_KEY_F9:
      return KeyCode::F9;
    case GLFW_KEY_F10:
      return KeyCode::F10;
    case GLFW_KEY_F11:
      return KeyCode::F11;
    case GLFW_KEY_F12:
      return KeyCode::F12;

    // Arrow s
    case GLFW_KEY_UP:
      return KeyCode::ArrowUp;
    case GLFW_KEY_DOWN:
      return KeyCode::ArrowDown;
    case GLFW_KEY_LEFT:
      return KeyCode::ArrowLeft;
    case GLFW_KEY_RIGHT:
      return KeyCode::ArrowRight;

    // Special s
    case GLFW_KEY_ESCAPE:
      return KeyCode::Escape;
    case GLFW_KEY_ENTER:
      return KeyCode::Enter;
    case GLFW_KEY_BACKSPACE:
      return KeyCode::Backspace;
    case GLFW_KEY_TAB:
      return KeyCode::Tab;
    case GLFW_KEY_SPACE:
      return KeyCode::Space;
    case GLFW_KEY_LEFT_SHIFT:
      return KeyCode::LeftShift;
    case GLFW_KEY_RIGHT_SHIFT:
      return KeyCode::RightShift;
    case GLFW_KEY_LEFT_CONTROL:
      return KeyCode::LeftControl;
    case GLFW_KEY_RIGHT_CONTROL:
      return KeyCode::RightControl;
    case GLFW_KEY_LEFT_ALT:
      return KeyCode::LeftAlt;
    case GLFW_KEY_RIGHT_ALT:
      return KeyCode::RightAlt;

    // Additional s
    case GLFW_KEY_INSERT:
      return KeyCode::Insert;
    case GLFW_KEY_DELETE:
      return KeyCode::Delete;
    case GLFW_KEY_HOME:
      return KeyCode::Home;
    case GLFW_KEY_END:
      return KeyCode::End;
    case GLFW_KEY_PAGE_UP:
      return KeyCode::PageUp;
    case GLFW_KEY_PAGE_DOWN:
      return KeyCode::PageDown;

    // Punctuation and symbols
    case GLFW_KEY_APOSTROPHE:
      return KeyCode::Apostrophe;
    case GLFW_KEY_COMMA:
      return KeyCode::Comma;
    case GLFW_KEY_MINUS:
      return KeyCode::Minus;
    case GLFW_KEY_PERIOD:
      return KeyCode::Period;
    case GLFW_KEY_SLASH:
      return KeyCode::Slash;
    case GLFW_KEY_SEMICOLON:
      return KeyCode::Semicolon;
    case GLFW_KEY_EQUAL:
      return KeyCode::Equal;
    case GLFW_KEY_LEFT_BRACKET:
      return KeyCode::LeftBracket;
    case GLFW_KEY_RIGHT_BRACKET:
      return KeyCode::RightBracket;
    case GLFW_KEY_BACKSLASH:
      return KeyCode::Backslash;
    case GLFW_KEY_GRAVE_ACCENT:
      return KeyCode::GraveAccent;

    default:
      return std::nullopt;
  }
}

constexpr util::EnumMapper<MouseBtnCode, int> kMouseKeyCodeGLFWMapping({
    {MouseBtnCode::MouseButtonLeft, GLFW_MOUSE_BUTTON_1},    //
    {MouseBtnCode::MouseButtonRight, GLFW_MOUSE_BUTTON_2},   //
    {MouseBtnCode::MouseButtonMiddle, GLFW_MOUSE_BUTTON_3},  //
    {MouseBtnCode::MouseButton4, GLFW_MOUSE_BUTTON_4},       //
    {MouseBtnCode::MouseButton5, GLFW_MOUSE_BUTTON_5},       //
    {MouseBtnCode::MouseButton6, GLFW_MOUSE_BUTTON_6},       //
    {MouseBtnCode::MouseButton7, GLFW_MOUSE_BUTTON_7},       //
    {MouseBtnCode::MouseButton8, GLFW_MOUSE_BUTTON_8}        //
});

inline int mouse_code_to_glfw(MouseBtnCode code) { return kMouseKeyCodeGLFWMapping[code]; }

inline std::optional<MouseBtnCode> mouse_code_from_glfw(int code) { return kMouseKeyCodeGLFWMapping.from_value(code); }

constexpr util::EnumMapper<CursorMode, int> kMouseCursorModeGLFWMapping({
    {CursorMode::Normal, GLFW_CURSOR_NORMAL},      //
    {CursorMode::Hidden, GLFW_CURSOR_HIDDEN},      //
    {CursorMode::Captured, GLFW_CURSOR_CAPTURED},  //
    {CursorMode::Disabled, GLFW_CURSOR_DISABLED},  //
});

inline int mouse_cursor_to_glfw(CursorMode code) { return kMouseCursorModeGLFWMapping[code]; }

inline std::optional<CursorMode> mouse_cursor_from_glfw(int code) {
  return kMouseCursorModeGLFWMapping.from_value(code);
}

}  // namespace eray::os
