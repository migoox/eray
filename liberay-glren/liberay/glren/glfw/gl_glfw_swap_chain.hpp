#pragma once

#include <GLFW/glfw3.h>

#include <liberay/os/window/window.hpp>

namespace eray::glren {

class GLFWSwapChain {
 public:
  GLFWSwapChain() = delete;
  explicit GLFWSwapChain(std::nullptr_t) {}

  static GLFWSwapChain create(const os::Window& window);
  void swap_buffers();
  void set_swap_interval(bool vsync);

  GLFWwindow* win_handle() const { return win_handle_; }

 private:
  explicit GLFWSwapChain(GLFWwindow* win_handle) : win_handle_(win_handle) {}

  GLFWwindow* win_handle_{};
};

}  // namespace eray::glren
