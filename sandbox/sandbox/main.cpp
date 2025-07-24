#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/zstring_view.hpp>
#include <map>
#include <variant>
#include <vector>
#include <version/version.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

struct GLFWWindowCreationFailure {};

struct VulkanExtensionNotSupported {
  std::string glfw_extension;
};
struct SomeOfTheRequestedVulkanLayersAreNotSupported {};
struct VulkanInstanceCreationFailure {
  vk::Result result;
};
struct VulkanDebugMessengerCreationFailure {
  vk::Result result;
};
struct FailedToEnumeratePhysicalDevices {
  vk::Result result;
};
struct NoSuitablePhysicalDevicesFound {};
struct VulkanLogicalDeviceCreationFailure {
  vk::Result result;
};
struct VulkanGraphicsQueueCreationFailure {
  vk::Result result;
};

using VulkanInitError = std::variant<VulkanExtensionNotSupported, VulkanInstanceCreationFailure,
                                     SomeOfTheRequestedVulkanLayersAreNotSupported, VulkanDebugMessengerCreationFailure,
                                     FailedToEnumeratePhysicalDevices, NoSuitablePhysicalDevicesFound,
                                     VulkanLogicalDeviceCreationFailure, VulkanGraphicsQueueCreationFailure>;
using AppError        = std::variant<GLFWWindowCreationFailure, VulkanInitError>;

class HelloTriangleApplication {
 public:
  std::expected<void, AppError> run() {
    TRY(initWindow());
    TRY(initVk())
    mainLoop();
    cleanup();

    return {};
  }

  static constexpr uint32_t kWinWidth  = 800;
  static constexpr uint32_t kWinHeight = 600;

 private:
  std::expected<void, VulkanInitError> initVk() {
    TRY(createVkInstance())
    TRY(setup_debug_messenger())
    TRY(pick_physical_device())
    TRY(create_logical_device())

    return {};
  }

  std::expected<void, GLFWWindowCreationFailure> initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    glfwSetErrorCallback([](int error_code, const char* description) {
      eray::util::Logger::err("GLFW Error #{0}: {1}", error_code, description);
    });

    window_ = glfwCreateWindow(kWinWidth, kWinHeight, "Vulkan", nullptr, nullptr);
    if (!window_) {
      return std::unexpected(GLFWWindowCreationFailure{});
    }

    eray::util::Logger::succ("Successfully created a GLFW Window");

    return {};
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
    }
  }

  void cleanup() {
    glfwDestroyWindow(window_);
    glfwTerminate();

    eray::util::Logger::succ("Finished cleanup");
  }

  std::expected<void, VulkanInitError> createVkInstance() {
    // To create a Vulkan Instance we specify
    //  1. app info
    //  2. global extensions (e.g. those needed by GLFW)
    //  3. validation layers (by default vulkan has no overhead, when debugging it's useful to do runtime checks if api
    //  is used correctly)

    // == 1. App info ==================================================================================================

    // Technically optional
    auto app_info = vk::ApplicationInfo{
        .pApplicationName   = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = vk::ApiVersion14  //
    };

    // == 2. Global Extensions =========================================================================================

    // Wrapper over vkEnumerateInstanceExtensionProperties. Returns global extension properties.
    // See: https://registry.khronos.org/vulkan/specs/latest/man/html/vkEnumerateInstanceExtensionProperties.html
    auto extension_props = context_.enumerateInstanceExtensionProperties();

    // Check if the required GLFW extensions are supported by the Vulkan implementation.
    auto required_extensions = get_instance_extensions();
    for (const auto& ext : required_extensions) {
      if (std::ranges::none_of(extension_props, [ext_name = std::string_view(ext)](const auto& prop) {
            return std::string_view(prop.extensionName) == ext_name;
          })) {
        eray::util::Logger::err("Required extension not supported: {}", ext);
        return std::unexpected(VulkanExtensionNotSupported{
            .glfw_extension = std::string(ext)  //
        });
      }
    }

    // == 3. Validation Layers =========================================================================================

    auto required_layers = get_instance_validation_layers();
    auto layer_props     = context_.enumerateInstanceLayerProperties();
    if (std::ranges::any_of(required_layers, [&layer_props](const auto& required_layer) {
          return std::ranges::none_of(layer_props, [required_layer](auto const& layer_prop) {
            return required_layer == std::string_view(layer_prop.layerName);
          });
        })) {
      eray::util::Logger::err("Failed to create a vulkan instance. Use of unsupported validation layer(s).");
      return std::unexpected(SomeOfTheRequestedVulkanLayersAreNotSupported());
    }

    // == Vulkan Instance Creation =====================================================================================

    auto create_info = vk::InstanceCreateInfo{
        // app info
        .pApplicationInfo = &app_info,
        // validation layers
        .enabledLayerCount   = static_cast<uint32_t>(required_layers.size()),
        .ppEnabledLayerNames = required_layers.data(),
        // global extensions
        .enabledExtensionCount   = static_cast<uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data(),  //
    };

    // When using VULKAN_HPP_NO_EXCEPTIONS, the vk::raii::Instance(context_, create_info) is unavailable,
    // as it may throw. For that reason you need to use context_.createInstance(). It wraps the vkCreateInstance:
    // https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateInstance.html
    if (auto instance_opt = context_.createInstance(create_info)) {
      instance_ = std::move(instance_opt.value());
    } else {
      auto err = vk::to_string(instance_opt.error());
      eray::util::Logger::err("Failed to create a vulkan instance. Error type: {}", err);
      return std::unexpected(VulkanInstanceCreationFailure{.result = instance_opt.error()});
    }

    eray::util::Logger::succ("Successfully created a Vulkan Instance");

    return {};
  }

  std::expected<void, VulkanDebugMessengerCreationFailure> setup_debug_messenger() {
    if (!kEnableValidationLayers) {
      return {};
    }
    auto severity_flags = vk::DebugUtilsMessageSeverityFlagsEXT(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    auto msg_type_flags = vk::DebugUtilsMessageTypeFlagsEXT(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    auto debug_utils_messenger_create_info_ext = vk::DebugUtilsMessengerCreateInfoEXT{
        .messageSeverity = severity_flags,
        .messageType     = msg_type_flags,
        .pfnUserCallback = &debug_callback  //
    };

    if (auto debug_messenger_opt = instance_.createDebugUtilsMessengerEXT(debug_utils_messenger_create_info_ext)) {
      debug_messenger_ = std::move(debug_messenger_opt.value());
    } else {
      auto err = vk::to_string(debug_messenger_opt.error());
      eray::util::Logger::err("Failed to create a vulkan debug messenger. Error type: {}", err);
      return std::unexpected(VulkanDebugMessengerCreationFailure{
          .result = debug_messenger_opt.error()  //
      });
    }

    return {};
  }

  std::vector<const char*> get_instance_extensions() {
    // GLFW has a function that returns Vulkan extension(s) that are needed to integrate GLFW with Vulkan
    uint32_t glfw_extensions_count = 0;
    auto* glfw_extensions          = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

    auto required_extensions = std::vector<const char*>(glfw_extensions, glfw_extensions + glfw_extensions_count);
    if (kEnableValidationLayers) {
      // The extension is needed to setup a debug messenger
      required_extensions.push_back(
          vk::EXTDebugUtilsExtensionName  // = VK_EXT_DEBUG_UTILS_EXTENSION_NAME = "VK_EXT_debug_utils"
      );
    }

    eray::util::Logger::info("Instance Extensions: {}", required_extensions);

    return required_extensions;
  }

  std::vector<const char*> get_instance_validation_layers() {
    auto required_layers = std::vector<const char*>();
    if (kEnableValidationLayers) {
      eray::util::Logger::info("Vulkan Validation Layers are enabled");
      required_layers.assign(kValidationLayers.begin(), kValidationLayers.end());
    }

    return required_layers;
  }

  std::expected<void, VulkanInitError> pick_physical_device() {
    auto devices_opt = instance_.enumeratePhysicalDevices();
    if (!devices_opt) {
      eray::util::Logger::err("Failed to enumerate physical devices. {}", vk::to_string(devices_opt.error()));
      return std::unexpected(FailedToEnumeratePhysicalDevices{.result = devices_opt.error()});
    }
    auto devices = devices_opt.value();

    if (devices.empty()) {
      eray::util::Logger::err("Failed to find GPUs with Vulkan support.");
      return std::unexpected(NoSuitablePhysicalDevicesFound{});
    }

    // Ordered map for automatic sorting by device score
    auto candidates = std::multimap<uint32_t, vk::raii::PhysicalDevice>();

    for (const auto& device : devices) {
      auto props    = device.getProperties();  // name, type, supported Vulkan version
      auto features = device.getFeatures();    // optional features like texture compression, 64-bit floats, multi-view
                                               // port rendering (VR)
      auto queue_families = device.getQueueFamilyProperties();
      auto extensions     = device.enumerateDeviceExtensionProperties();

      if (!features.geometryShader || !features.tessellationShader) {
        continue;
      }

      if (std::ranges::find_if(queue_families, [](const vk::QueueFamilyProperties& qfp) {
            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
          }) == queue_families.end()) {
        continue;
      }

      auto found = true;
      for (const auto& extension : kDeviceExtensions) {
        found = found && std::ranges::find_if(extensions, [extension](const auto& ext) {
                           return std::string_view(extension) == std::string_view(ext.extensionName);
                         }) != extensions.end();
        if (!found) {
          break;
        }
      }
      if (!found) {
        continue;
      }

      uint32_t score = 0;
      if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += 10000;
      }
      score += props.limits.maxImageDimension2D;

      candidates.emplace(score, device);
    }

    if (candidates.empty()) {
      eray::util::Logger::err("Failed to find GPUs that meet the requirements.");
      return std::unexpected(NoSuitablePhysicalDevicesFound{});
    }

    auto candidates_str = std::string("Physical Device (GPU) Candidates:");
    for (const auto& candidate : candidates) {
      candidates_str += std::format("\nScore: {}, Device Name: {}", candidate.first,
                                    std::string_view(candidate.second.getProperties().deviceName));
    }
    eray::util::Logger::info("{}", candidates_str);

    // Pick the best GPU candidate
    physical_device_ = candidates.rbegin()->second;

    eray::util::Logger::succ("Successfully picked a physical device with name {}",
                             std::string_view(physical_device_.getProperties().deviceName));

    return {};
  }

  std::expected<void, VulkanInitError> create_logical_device() {
    // == 1. Setup the features ========================================================================================

    // Most of the Vulkan structures contain pNext that allows for chaining the structures into a linked list
    // The C++ API provides a strongly typed structure chain which is safer as the types of structure in the chain
    // are known at compile time. The StructureChain inherits from std::tuple, the pNext is set for compatibility
    // with the C API.
    auto feature_chain = vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                                            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>{
        {},                                 // vk::PhysicalDeviceFeatures2
        {.dynamicRendering = vk::True},     // Enable dynamic rendering from Vulkan 1.3
        {.extendedDynamicState = vk::True}  // Enable extended dynamic state from the extension
    };

    // == 2. Setup Graphics Queue ======================================================================================

    auto queue_family_props = physical_device_.getQueueFamilyProperties();

    auto graphics_queue_family_prop_it = std::ranges::find_if(queue_family_props, [](const auto& prop) {
      return (prop.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
    });

    auto graphics_index =
        static_cast<uint32_t>(std::distance(queue_family_props.begin(), graphics_queue_family_prop_it));

    float queue_priority          = 0.F;
    auto device_queue_create_info = vk::DeviceQueueCreateInfo{
        .queueFamilyIndex = graphics_index,
        .queueCount       = 1,
        .pQueuePriorities = &queue_priority,  //
    };

    // == 3. Logical Device Creation ===================================================================================

    auto device_create_info = vk::DeviceCreateInfo{
        .pNext                   = &feature_chain.get<vk::PhysicalDeviceFeatures2, 0>(),  // connect the feature chain
        .queueCreateInfoCount    = 1,                                                     //
        .pQueueCreateInfos       = &device_queue_create_info,                             //
        .enabledExtensionCount   = static_cast<uint32_t>(kDeviceExtensions.size()),
        .ppEnabledExtensionNames = kDeviceExtensions.data(),
    };

    if (auto result = physical_device_.createDevice(device_create_info)) {
      device_ = std::move(*result);
    } else {
      eray::util::Logger::err("Failed to create a logical device. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanLogicalDeviceCreationFailure{.result = result.error()});
    }

    // == 4. Graphics Queue Creation ===================================================================================

    // Because we’re only creating a single queue from this family, we’ll simply use index 0
    if (auto result = device_.getQueue(graphics_index, 0)) {
      graphics_queue_ = std::move(*result);
    } else {
      eray::util::Logger::err("Failed to create a graphics queue. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanGraphicsQueueCreationFailure{.result = result.error()});
    }

    eray::util::Logger::succ("Successfully created a logical device and a graphics queue.");

    return {};
  }

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                         vk::DebugUtilsMessageTypeFlagsEXT type,
                                                         const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                         void*) {
    switch (severity) {
      case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
      case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
        eray::util::Logger::info("Vulkan Debug (Type: {}): {}", vk::to_string(type), p_callback_data->pMessage);
        return vk::True;
      case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
        eray::util::Logger::warn("Vulkan Debug (Type: {}): {}", vk::to_string(type), p_callback_data->pMessage);
        return vk::True;
      case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
        eray::util::Logger::err("Vulkan Debug (Type: {}): {}", vk::to_string(type), p_callback_data->pMessage);
        return vk::True;
      default:
        return vk::False;
    };
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

  /**
   * @brief When VK_EXT_debug_utils is used, debug_messenger allows to set debug callback to integrate the Vulkan API
   * with logger.
   *
   */
  vk::raii::DebugUtilsMessengerEXT debug_messenger_ = nullptr;

  /**
   * @brief Represents a GPU. Used to query physical GPU details, like features, capabilities, memory size, etc.
   *
   */
  vk::raii::PhysicalDevice physical_device_ = nullptr;

  /**
   * @brief The “logical” GPU context that you actually execute things on. It allows for interaction with the GPU.
   *
   */
  vk::raii::Device device_ = nullptr;

  /**
   * @brief Any graphics command might be submitted to this queue.
   *
   */
  vk::raii::Queue graphics_queue_ = nullptr;

  /**
   * @brief GLFW window pointer.
   *
   */
  GLFWwindow* window_ = nullptr;

/**
 * @brief Validation layers are optional components that hook into Vulkan function calls to apply additional
 * operations. Common operations in validation layers are:
 *  - Checking the values of parameters against the specification to detect misuse
 *  - Tracking the creation and destruction of objects to find resource leaks
 *  - Checking thread safety by tracking the threads that calls originate from
 *  - Logging every call and its parameters to the standard output
 *  - Tracing Vulkan calls for profiling and replaying
 *
 */
#ifdef NDEBUG
  static constexpr bool kEnableValidationLayers = false;
#else
  static constexpr bool kEnableValidationLayers = true;
#endif

  static constexpr std::array<const char*, 1> kValidationLayers = {"VK_LAYER_KHRONOS_validation"};

  /**
   * @brief We provide the extensions to the logical device. The physical device might be queried if these extensions
   * are supported.
   *
   */
  static constexpr std::array<const char*, 4> kDeviceExtensions = {
      vk::KHRSwapchainExtensionName,         // requires Surface Instance Extension
      vk::KHRSpirv14ExtensionName,           //
      vk::KHRSynchronization2ExtensionName,  //
      vk::KHRCreateRenderpass2ExtensionName  //
  };
};

int main() {
  eray::util::Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());
  eray::util::Logger::instance().set_abs_build_path(ERAY_BUILD_ABS_PATH);

  auto app = HelloTriangleApplication();
  if (auto result = app.run(); !result) {
    eray::util::panic("Error");
  }

  return 0;
}
