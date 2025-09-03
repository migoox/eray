#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <expected>
#include <liberay/glren/gl_error.hpp>
#include <liberay/glren/glfw/gl_glfw_window_creator.hpp>
#include <liberay/os/error.hpp>
#include <liberay/os/window/glfw/glfw_window.hpp>
#include <memory>

namespace eray::os {

Result<std::unique_ptr<IWindowCreator>, Error> OpenGLGLFWWindowCreator::create() {
  util::Logger::info("Initializing GLFW backend...");

  const int status = glfwInit();
  if (!status) {
    util::Logger::err("Could not initialize GLFW backend");
    return std::unexpected(Error{
        .msg  = "GLFW init failed",
        .code = ErrorCode::WindowBackendCreationFailure{},
    });
  }

  glfwSetErrorCallback([](int error_code, const char* description) {
    eray::util::Logger::err("GLFW Error #{0}: {1}", error_code, description);
  });

  util::Logger::succ("Successfully initialized GLFW backend");

  return std::make_unique<OpenGLGLFWWindowCreator>();
}

Result<std::unique_ptr<Window>, Error> OpenGLGLFWWindowCreator::create_window(const WindowProperties& props) {
#ifndef NDEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  auto* window = glfwCreateWindow(props.width, props.height, props.title.c_str(), nullptr, nullptr);
  if (!window) {
    return std::unexpected(Error{
        .msg  = "GLFW window creation failed",
        .code = ErrorCode::WindowBackendFailure{},
    });
  }
  glfwMakeContextCurrent(window);
  const int glad_version = gladLoadGL();
  if (glad_version == 0) {
    return std::unexpected(Error{
        .msg  = "GLAD gl loading failed",
        .code = ErrorCode::RenderingAPIInitializationFailure{},
    });
  }

  util::Logger::info("OpenGL info:");
  util::Logger::info("\tVendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
  util::Logger::info("\tRenderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
  util::Logger::info("\tVersion: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  GLint max_uniform_block_size     = 0;
  GLint max_uniform_block_bindings = 0;
  ERAY_GL_CALL(glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &max_uniform_block_size));
  ERAY_GL_CALL(glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_uniform_block_bindings));
  util::Logger::info("\tMax uniform block size: {}", max_uniform_block_size);
  util::Logger::info("\tMax uniform block bindings: {}", max_uniform_block_bindings);

  return std::unique_ptr<Window>(new GLFWWindow(window, props, rendering_api(), window_api()));
}

OpenGLGLFWWindowCreator::~OpenGLGLFWWindowCreator() { glfwTerminate(); }

}  // namespace eray::os
