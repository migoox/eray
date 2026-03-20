#pragma once
#include <cstdint>
#include <string>

namespace eray::os {

struct WindowProperties {
  std::string title = "Window";
  bool vsync;
  bool fullscreen;
  uint32_t width;
  uint32_t height;
  // TODO(migoox): load from file
};

}  // namespace eray::os
