#include <GLFW/glfw3.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <liberay/os/rendering_api.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/glfw/glfw_mappings.hpp>
#include <liberay/os/window/glfw/glfw_window.hpp>
#include <liberay/os/window/mouse_cursor_codes.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/logger.hpp>

namespace eray::os {

namespace glfw {

namespace {

inline GLFWwindow* glfw_win_ptr(void* glfw_window_ptr) { return reinterpret_cast<GLFWwindow*>(glfw_window_ptr); }

}  // namespace

}  // namespace glfw

GLFWWindow::GLFWWindow(void* glfw_window_ptr, const WindowProperties& props, RenderingAPI rendering_api,
                       WindowAPI window_api)
    : Window(props), glfw_window_ptr_(glfw_window_ptr), rendering_api_(rendering_api), window_api_(window_api) {
  //   glfwSwapInterval(props.vsync ? 1 : 0); opengl specific
  init_dispatcher();
}

GLFWWindow::~GLFWWindow() {
  util::Logger::info("Destroying GLFW window...");
  glfwDestroyWindow(glfw::glfw_win_ptr(glfw_window_ptr_));
  util::Logger::succ("GLFW window destroyed");
}

void GLFWWindow::init_dispatcher() {
  glfwSetWindowUserPointer(glfw::glfw_win_ptr(glfw_window_ptr_), reinterpret_cast<void*>(this));

  glfwSetWindowCloseCallback(glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window) {
    auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;
    dispatcher->enqueue_event(WindowClosedEvent());
  });

  glfwSetWindowSizeCallback(glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window, int width, int height) {
    auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;
    dispatcher->dispatch_event(WindowResizedEvent(width, height));
  });

  glfwSetFramebufferSizeCallback(
      glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window, int /*width*/, int /*height*/) {
        auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;
        dispatcher->dispatch_event(FramebufferResizedEvent());
      });

  glfwSetWindowFocusCallback(glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window, int focused) {
    auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;
    if (focused) {
      dispatcher->enqueue_event(WindowFocusedEvent());
    } else {
      dispatcher->enqueue_event(WindowLostFocusEvent());
    }
  });

  glfwSetWindowPosCallback(glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window, int xpos, int ypos) {
    auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;
    dispatcher->dispatch_event(WindowMovedEvent(xpos, ypos));
  });

  glfwSetKeyCallback(glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window, int key_code, int /*scancode*/,
                                                              int action, int /*mods*/) {
    auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;

    auto key_val = key_code_from_glfw(key_code);
    if (!key_val) {
      return;
    }
    if (action == GLFW_PRESS) {
      dispatcher->enqueue_event(KeyPressedEvent(*key_val));
    }
    if (action == GLFW_RELEASE) {
      dispatcher->enqueue_event(KeyReleasedEvent(*key_val));
    }
  });

  glfwSetMouseButtonCallback(
      glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window, int button_code, int action, int /*mods*/) {
        auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;

        auto button = mouse_code_from_glfw(button_code);
        if (!button) {
          return;
        }

        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(window, &x, &y);
        ImGuiIO& io = ImGui::GetIO();
        if (action == GLFW_PRESS) {
          dispatcher->enqueue_event(MouseButtonPressedEvent(*button, x, y, io.WantCaptureMouse));
        }
        if (action == GLFW_RELEASE) {
          dispatcher->enqueue_event(MouseButtonReleasedEvent(*button, x, y, io.WantCaptureMouse));
        }
      });

  glfwSetScrollCallback(glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window, double xoffset, double yoffset) {
    auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;
    dispatcher->enqueue_event(MouseScrolledEvent(xoffset, yoffset));
  });

  glfwSetCursorEnterCallback(glfw::glfw_win_ptr(glfw_window_ptr_), [](GLFWwindow* window, int entered) {
    auto* dispatcher = &reinterpret_cast<GLFWWindow*>(glfwGetWindowUserPointer(window))->event_dispatcher_;
    if (entered) {
      dispatcher->enqueue_event(MouseEntered());
    } else {
      dispatcher->enqueue_event(MouseLeft());
    }
  });
}

void GLFWWindow::set_title(util::zstring_view title) {
  glfwSetWindowTitle(glfw::glfw_win_ptr(glfw_window_ptr_), title.c_str());
  props_.title = std::string(title);
}

void GLFWWindow::set_window_size(int width, int height) {
  glfwSetWindowSize(glfw::glfw_win_ptr(glfw_window_ptr_), width, height);
  props_.width  = width;
  props_.height = height;
}

void GLFWWindow::set_vsync(bool /* vsync */) {  // NOLINT
  // TODO(migoox): add vsync switch support
  util::not_impl_yet();
}

void GLFWWindow::set_fullscreen(bool /* fullscreen */) {  // NOLINT
  // TODO(migoox): add fullscreen support
  util::not_impl_yet();
}

Window::MousePosition GLFWWindow::mouse_pos() const {
  auto pos = MousePosition{.x = 1.0, .y = 1.0};
  glfwGetCursorPos(glfw::glfw_win_ptr(glfw_window_ptr_), &pos.x, &pos.y);
  return pos;
}

bool GLFWWindow::is_btn_held(KeyCode code) {
  // TODO(migoox): use array of booleans instead
  return glfwGetKey(glfw::glfw_win_ptr(glfw_window_ptr_), key_code_to_glfw(code)) == GLFW_PRESS;
}

bool GLFWWindow::is_mouse_btn_held(MouseBtnCode code) {
  // TODO(migoox): use array of booleans instead
  return glfwGetMouseButton(glfw::glfw_win_ptr(glfw_window_ptr_), mouse_code_to_glfw(code)) == GLFW_PRESS;
}

void GLFWWindow::set_mouse_cursor_mode(CursorMode cursor_mode) {
  glfwSetInputMode(glfw::glfw_win_ptr(glfw_window_ptr_), GLFW_CURSOR, mouse_cursor_to_glfw(cursor_mode));
}

CursorMode GLFWWindow::mouse_cursor_mode() {
  return *mouse_cursor_from_glfw(glfwGetInputMode(glfw::glfw_win_ptr(glfw_window_ptr_), GLFW_CURSOR));
}

bool GLFWWindow::should_close() const { return glfwWindowShouldClose(glfw::glfw_win_ptr(glfw_window_ptr_)) != 0; }

Window::Dimensions GLFWWindow::framebuffer_size() const {
  auto size = Window::Dimensions{.width = 0, .height = 0};
  glfwGetFramebufferSize(glfw::glfw_win_ptr(glfw_window_ptr_), &size.width, &size.height);
  while (size.width == 0 || size.height == 0) {
    glfwGetFramebufferSize(glfw::glfw_win_ptr(glfw_window_ptr_), &size.width, &size.height);
    glfwWaitEvents();
  }

  return size;
}

void GLFWWindow::poll_events() { glfwPollEvents(); }

}  // namespace eray::os
