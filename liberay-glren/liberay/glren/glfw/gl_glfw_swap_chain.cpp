#include <GLFW/glfw3.h>

#include <liberay/glren/glfw/gl_glfw_swap_chain.hpp>

namespace eray::glren {

GLFWSwapChain GLFWSwapChain::create(const os::Window& window) {
  if (window.window_api() != os::WindowAPI::GLFW) {
    util::panic("Only GLFW API is supported by the renderer");
  }
  auto* glfw_win_handle = reinterpret_cast<GLFWwindow*>(window.win_handle());  // required for buffer swapping

  return GLFWSwapChain(glfw_win_handle);
}

void GLFWSwapChain::swap_buffers() { glfwSwapBuffers(win_handle_); }

void GLFWSwapChain::set_swap_interval(bool vsync) { glfwSwapInterval(vsync ? 1 : 0); }

}  // namespace eray::glren
