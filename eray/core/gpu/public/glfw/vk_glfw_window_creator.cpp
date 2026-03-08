#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <liberay/os/error.hpp>
#include <liberay/os/window/glfw/glfw_window.hpp>
#include <liberay/vkren/glfw/vk_glfw_window_creator.hpp>
#include <memory>

namespace eray::os {

Result<std::unique_ptr<IWindowCreator>, Error> VulkanGLFWWindowCreator::create() {
  util::Logger::info("Initializing GLFW backend...");

  const int status = glfwInit();
  if (!status) {
    util::Logger::err("Could not initialize GLFW backend");
    return std::unexpected(Error{
        .msg  = "GLFW init failed",
        .code = ErrorCode::WindowBackendCreationFailure{},
    });
  }

  if (!glfwVulkanSupported()) {
    eray::util::panic("GLFW could not load Vulkan");
  }

  glfwSetErrorCallback([](int error_code, const char* description) {
    eray::util::Logger::err("GLFW Error #{0}: {1}", error_code, description);
  });

  util::Logger::succ("Successfully initialized GLFW backend");

  return std::make_unique<VulkanGLFWWindowCreator>();
}

Result<std::unique_ptr<Window>, Error> VulkanGLFWWindowCreator::create_window(const WindowProperties& props) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  auto* window = glfwCreateWindow(props.width, props.height, props.title.c_str(), nullptr, nullptr);
  if (!window) {
    return std::unexpected(Error{
        .msg  = "GLFW window creation failed",
        .code = ErrorCode::WindowBackendFailure{},
    });
  }

  return std::unique_ptr<Window>(new GLFWWindow(window, props, window_api()));
}

void VulkanGLFWWindowCreator::terminate() {
  util::Logger::info("Terminating GLFW backend...");
  glfwTerminate();
  util::Logger::succ("Successfully terminated GLFW backend");
}

}  // namespace eray::os
