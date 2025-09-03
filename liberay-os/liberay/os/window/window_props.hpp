#pragma once
#include <string>

namespace eray::os {

struct WindowProperties {
  std::string title = "Window";
  bool vsync;
  bool fullscreen;
  int width;
  int height;
  // TODO(migoox): load from file
};

}  // namespace eray::os
