#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <variant>
#include <version/version.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

struct GLFWExtensionNotSupportedError {
  std::string glfw_extension;
};
struct VulkanInstanceCreationFailed {};

using VulkanInitError = std::variant<GLFWExtensionNotSupportedError, VulkanInstanceCreationFailed>;
using AppError        = std::variant<VulkanInitError>;

class HelloTriangleApplication {
 public:
  std::expected<void, AppError> run() {
    TRY(initVk())

    initWindow();
    mainLoop();
    cleanup();

    return {};
  }

 private:
  std::expected<void, VulkanInitError> initVk() {
    TRY(createVkInstance())

    return {};
  }

  void initWindow() {}

  void mainLoop() {}

  void cleanup() { eray::util::Logger::succ("Finished cleanup"); }

  std::expected<void, VulkanInitError> createVkInstance() {
    // Technically optional
    auto app_info = vk::ApplicationInfo{
        .pApplicationName   = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = vk::ApiVersion14  //
    };

    // Wrapper over vkEnumerateInstanceExtensionProperties. Returns up to requested number of global extension
    // properties.
    // see: https://registry.khronos.org/vulkan/specs/latest/man/html/vkEnumerateInstanceExtensionProperties.html
    auto extension_props = context_.enumerateInstanceExtensionProperties();

    // GLFW has a function that returns Vulkan extension(s) that are needed to integrate GLFW with Vulkan
    uint32_t glfw_extensions_count = 0;
    auto* glfw_extensions          = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

    // Check if the required GLFW extensions are supported by the Vulkan implementation.
    for (auto i = 0U; i < glfw_extensions_count; ++i) {
      if (std::ranges::none_of(extension_props,
                               [ext_name = std::string_view(glfw_extensions[i])](const vk::ExtensionProperties& prop) {
                                 return std::string_view(prop.extensionName) == ext_name;
                               })) {
        eray::util::Logger::err("Required GLFW extension not supported: {}", glfw_extensions[i]);
        return std::unexpected(GLFWExtensionNotSupportedError{
            .glfw_extension = std::string(glfw_extensions[i])  //
        });
      }
    }

    // This struct is NOT optional. It allows to specify:
    //  - global extensions (e.g. those used by glfw)
    //  - validation layers (by default vulkan has no overhead, when debugging it's useful to do runtime checks if api
    //  is used correctly)
    //  - app info
    auto create_info = vk::InstanceCreateInfo{
        .pApplicationInfo        = &app_info,
        .enabledExtensionCount   = glfw_extensions_count,
        .ppEnabledExtensionNames = glfw_extensions,
    };

    // When using VULKAN_HPP_NO_EXCEPTIONS, the vk::raii::Instance(context_, create_info) is unavailable,
    // as it may throw. For that reason you need to use context_.createInstance(). It wraps the vkCreateInstance:
    // https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateInstance.html
    if (auto instance_opt = context_.createInstance(create_info)) {
      instance_ = std::move(instance_opt.value());
    } else {
      eray::util::Logger::err("Failed to create a vulkan instance.");
      return std::unexpected(VulkanInitError{});
    }

    eray::util::Logger::succ("Vulkan Instance has been created");
    return {};
  }

 private:
  /**
   * @brief Responsible for dynamic loading of the Vulkan library, it's a starting point.
   * It's a starting point for creating other RAII-based Vulkan objects like vk::raii::Instance or vk::raii::Device.
   * e.g. context_.createInstance(...) wraps C-style vkCreateInstance.
   *
   */
  vk::raii::Context context_;

  /**
   * @brief The Vulkan context, used to access drivers.
   *
   */
  vk::raii::Instance instance_ = nullptr;
};

int main() {
  eray::util::Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());
  eray::util::Logger::instance().set_abs_build_path(ERAY_BUILD_ABS_PATH);

  HelloTriangleApplication app;
  if (auto result = app.run(); !result) {
    eray::util::panic("Error");
  }

  return 0;
}
