#include <GLFW/glfw3.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <liberay/math/vec.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/glfw/glfw_mappings.hpp>
#include <liberay/os/window/glfw/glfw_window.hpp>
#include <liberay/os/window/mouse_cursor_codes.hpp>
#include <liberay/util/logger.hpp>

namespace eray::os {

namespace glfw {

namespace {

inline GLFWwindow* win_native(void* glfw_window_ptr) { return reinterpret_cast<GLFWwindow*>(glfw_window_ptr); }

}  // namespace

}  // namespace glfw

GLFWWindow::GLFWWindow(void* glfw_window_ptr, WindowProperties props, Driver driver,
                       std::unique_ptr<ImGuiBackend> imgui)
    : Window(std::move(props)), glfw_window_ptr_(glfw_window_ptr), driver_(driver), imgui_(std::move(imgui)) {
  if (props_.has_valid_pos) {
    glfwSetWindowPos(glfw::win_native(glfw_window_ptr_), props.pos.x, props.pos.y);
  } else {
    int x, y;  // NOLINT
    glfwGetWindowPos(glfw::win_native(glfw_window_ptr_), &x, &y);
    props_.pos = math::Vec2i(x, y);
  }
  glfwSwapInterval(props.vsync ? 1 : 0);

  init_dispatcher();
  imgui_->init_driver(glfw_window_ptr_);
}

GLFWWindow::~GLFWWindow() {
  util::Logger::info("Destroying GLFW window...");
  glfwDestroyWindow(glfw::win_native(glfw_window_ptr_));
  util::Logger::succ("GLFW window destroyed");
}

void GLFWWindow::init_dispatcher() {
  glfwSetWindowUserPointer(glfw::win_native(glfw_window_ptr_), reinterpret_cast<void*>(&event_dispatcher_));

  glfwSetWindowCloseCallback(glfw::win_native(glfw_window_ptr_), [](GLFWwindow* window) {
    auto* dispatcher = reinterpret_cast<WindowEventDispatcher*>(glfwGetWindowUserPointer(window));
    dispatcher->enqueue_event(WindowClosedEvent());
  });

  glfwSetWindowSizeCallback(glfw::win_native(glfw_window_ptr_), [](GLFWwindow* window, int width, int height) {
    auto* dispatcher = reinterpret_cast<WindowEventDispatcher*>(glfwGetWindowUserPointer(window));
    dispatcher->dispatch_event(WindowResizedEvent(width, height));
  });

  glfwSetWindowFocusCallback(glfw::win_native(glfw_window_ptr_), [](GLFWwindow* window, int focused) {
    auto* dispatcher = reinterpret_cast<WindowEventDispatcher*>(glfwGetWindowUserPointer(window));
    if (focused) {
      dispatcher->enqueue_event(WindowFocusedEvent());
    } else {
      dispatcher->enqueue_event(WindowLostFocusEvent());
    }
  });

  glfwSetWindowPosCallback(glfw::win_native(glfw_window_ptr_), [](GLFWwindow* window, int xpos, int ypos) {
    auto* dispatcher = reinterpret_cast<WindowEventDispatcher*>(glfwGetWindowUserPointer(window));
    dispatcher->dispatch_event(WindowMovedEvent(xpos, ypos));
  });

  glfwSetKeyCallback(glfw::win_native(glfw_window_ptr_),
                     [](GLFWwindow* window, int key_code, int /*scancode*/, int action, int /*mods*/) {
                       auto* dispatcher = reinterpret_cast<WindowEventDispatcher*>(glfwGetWindowUserPointer(window));

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
      glfw::win_native(glfw_window_ptr_), [](GLFWwindow* window, int button_code, int action, int /*mods*/) {
        auto* dispatcher = reinterpret_cast<WindowEventDispatcher*>(glfwGetWindowUserPointer(window));

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

  glfwSetScrollCallback(glfw::win_native(glfw_window_ptr_), [](GLFWwindow* window, double xoffset, double yoffset) {
    auto* dispatcher = reinterpret_cast<WindowEventDispatcher*>(glfwGetWindowUserPointer(window));
    dispatcher->enqueue_event(MouseScrolledEvent(xoffset, yoffset));
  });

  glfwSetCursorEnterCallback(glfw::win_native(glfw_window_ptr_), [](GLFWwindow* window, int entered) {
    auto* dispatcher = reinterpret_cast<WindowEventDispatcher*>(glfwGetWindowUserPointer(window));
    if (entered) {
      dispatcher->enqueue_event(MouseEntered());
    } else {
      dispatcher->enqueue_event(MouseLeft());
    }
  });
}

void GLFWWindow::update() {
  glfwPollEvents();
  if (driver_ == Driver::OpenGL) {
    // From docs: This function does not apply to Vulkan. If you are rendering with Vulkan, see `vkQueuePresentKHR`.
    glfwSwapBuffers(glfw::win_native(glfw_window_ptr_));
  }
}

void GLFWWindow::set_title(zstring_view title) {
  glfwSetWindowTitle(glfw::win_native(glfw_window_ptr_), title.c_str());
  props_.title = std::string(title);
}

void GLFWWindow::set_pos(math::Vec2i pos) {
  glfwSetWindowPos(glfw::win_native(glfw_window_ptr_), pos.x, pos.y);
  props_.pos = pos;
}

void GLFWWindow::set_size(math::Vec2i size) {
  glfwSetWindowSize(glfw::win_native(glfw_window_ptr_), size.x, size.y);
  props_.size = size;
}

void GLFWWindow::set_vsync(bool /* vsync */) {  // NOLINT
  // TODO(migoox): add vsync switch support
  util::not_impl_yet();
}

void GLFWWindow::set_fullscreen(bool /* fullscreen */) {  // NOLINT
  // TODO(migoox): add fullscreen support
  util::not_impl_yet();
}

math::Vec2d GLFWWindow::mouse_pos() const {
  auto pos = math::Vec2d(1.0, 2.0);
  glfwGetCursorPos(glfw::win_native(glfw_window_ptr_), &pos.x, &pos.y);
  return pos;
}

bool GLFWWindow::is_btn_held(KeyCode code) {
  // TODO(migoox): use array of booleans instead
  return glfwGetKey(glfw::win_native(glfw_window_ptr_), key_code_to_glfw(code)) == GLFW_PRESS;
}

bool GLFWWindow::is_mouse_btn_held(MouseBtnCode code) {
  // TODO(migoox): use array of booleans instead
  return glfwGetMouseButton(glfw::win_native(glfw_window_ptr_), mouse_code_to_glfw(code)) == GLFW_PRESS;
}

void GLFWWindow::set_mouse_cursor_mode(CursorMode cursor_mode) {
  glfwSetInputMode(glfw::win_native(glfw_window_ptr_), GLFW_CURSOR, mouse_cursor_to_glfw(cursor_mode));
}

CursorMode GLFWWindow::get_mouse_cursor_mode() {
  return *mouse_cursor_from_glfw(glfwGetInputMode(glfw::win_native(glfw_window_ptr_), GLFW_CURSOR));
}

}  // namespace eray::os
