#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <expected>
#include <liberay/os/driver.hpp>
#include <liberay/os/imgui_backend.hpp>
#include <liberay/os/window/glfw/glfw_window.hpp>
#include <liberay/os/window/glfw/glfw_window_backend.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>

namespace eray::os {

namespace glfw {

namespace {

bool init_opengl_ctx(GLFWwindow* window_ptr_) {
  glfwMakeContextCurrent(window_ptr_);
  const int glad_version = gladLoadGL(glfwGetProcAddress);
  if (glad_version == 0) {
    return false;
  }

  util::Logger::info("OpenGL info:");
  util::Logger::info("\tGLAD version: {0}.{1}", GLAD_VERSION_MAJOR(glad_version), GLAD_VERSION_MINOR(glad_version));
  util::Logger::info("\tVendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
  util::Logger::info("\tRenderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
  util::Logger::info("\tVersion: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  GLint max_uniform_block_size     = 0;
  GLint max_uniform_block_bindings = 0;
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_uniform_block_size);
  glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_uniform_block_bindings);
  util::Logger::info("\tMax uniform block size: {}", max_uniform_block_size);
  util::Logger::info("\tMax uniform block bindings: {}", max_uniform_block_bindings);

  return true;
}

bool init_vulcan_ctx(GLFWwindow* /*window_ptr_*/) {  // NOLINT
  util::Logger::err("Vulcan driver is not supported yet");
  return false;
}

}  // namespace

}  // namespace glfw

std::expected<std::unique_ptr<GLFWWindowBackend>, GLFWWindowBackend::BackendCreationError> GLFWWindowBackend::create(
    Driver driver) {
  util::Logger::info("Initializing GLFW backend...");

  if (driver != Driver::OpenGL && driver != Driver::Vulcan) {
    util::Logger::err("Provided driver ({}) is not supported by GLFW backend. Supported are {} and {}",
                      kDriverName[driver], kDriverName[Driver::OpenGL], kDriverName[Driver::Vulcan]);
    return std::unexpected(BackendCreationError::DriverIsNotSupported);
  }

  const int status = glfwInit();
  if (!status) {
    util::Logger::err("Could not initialize GLFW backend");
    return std::unexpected(BackendCreationError::InitializationError);
  }

  glfwSetErrorCallback([](int error_code, const char* description) {
    util::Logger::err("GLFW Error #{0}: {1}", error_code, description);
  });

  util::Logger::succ("Successfully initialized GLFW backend");

  return std::unique_ptr<GLFWWindowBackend>(new GLFWWindowBackend(driver));
}

GLFWWindowBackend::GLFWWindowBackend(Driver driver) : driver_(driver) {}

std::expected<std::unique_ptr<Window>, IWindowBackend::WindowCreationError> GLFWWindowBackend::create_window(
    WindowProperties props) {
  util::Logger::info("Creating a GLFW window...");
#ifdef IS_DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  // TODO(migoox): Handle fullscreen
  GLFWwindow* window_ptr = glfwCreateWindow(props.size.x, props.size.y, props.title.c_str(), nullptr, nullptr);
  if (driver_ == Driver::OpenGL) {
    if (!glfw::init_opengl_ctx(window_ptr)) {
      return std::unexpected(IWindowBackend::WindowCreationError::FailedToInitializeDriverContext);
    }
  } else if (driver_ == Driver::Vulcan) {
    if (!glfw::init_vulcan_ctx(window_ptr)) {
      return std::unexpected(IWindowBackend::WindowCreationError::FailedToInitializeDriverContext);
    }
  } else {
    util::Logger::err("Driver context with name {} is not supported by GLFW backend", kDriverName[driver_]);
    return std::unexpected(IWindowBackend::WindowCreationError::FailedToInitializeDriverContext);
  }

  util::Logger::succ("Created GLFW window with {} driver context", kDriverName[Driver::OpenGL]);

  auto imgui_glfw = ImGuiGLFWBackend::create(driver_);

  return std::unique_ptr<Window>(new GLFWWindow(window_ptr, std::move(props), driver_, std::move(imgui_glfw.value())));
}

GLFWWindowBackend::~GLFWWindowBackend() {
  util::Logger::info("Terminating GLFW backend...");
  glfwTerminate();
  util::Logger::succ("Successfully terminated GLFW backend");
}

}  // namespace eray::os
