#pragma once
#include <liberay/math/vec.hpp>
#include <string>

namespace eray::os {

struct WindowProperties {
  std::string title = "Window";
  bool vsync;
  bool fullscreen;
  math::Vec2i size   = math::Vec2i(1280, 720);
  math::Vec2i pos    = math::Vec2i(0, 0);
  bool has_valid_pos = false;
  // TODO(migoox): load from file
};

}  // namespace eray::os
