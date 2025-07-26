#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/zstring_view.hpp>
#include <limits>
#include <map>
#include <ranges>
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
struct VulkanUnsupportedQueueFamily {
  std::string queue_family_name;
};
struct VulkanLogicalDeviceCreationFailure {
  vk::Result result;
};
struct VulkanQueueCreationFailure {
  vk::Result result;
};
struct VulkanSwapChainCreationFailure {
  vk::Result result;
};

struct VulkanSurfaceCreationFailure {};

struct VulkanSwapChainSupportIsNotSufficient {};

using VulkanInitError =
    std::variant<VulkanExtensionNotSupported, VulkanInstanceCreationFailure,
                 SomeOfTheRequestedVulkanLayersAreNotSupported, VulkanDebugMessengerCreationFailure,
                 FailedToEnumeratePhysicalDevices, NoSuitablePhysicalDevicesFound, VulkanLogicalDeviceCreationFailure,
                 VulkanQueueCreationFailure, VulkanSurfaceCreationFailure, VulkanUnsupportedQueueFamily,
                 VulkanSwapChainSupportIsNotSufficient, VulkanSwapChainCreationFailure>;
using AppError = std::variant<GLFWWindowCreationFailure, VulkanInitError>;

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
    TRY(create_surface())
    TRY(pick_physical_device())
    TRY(create_logical_device())
    TRY(create_swap_chain())

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

    // == 2. Find Required Queue Families ==============================================================================

    {
      auto queue_family_props         = physical_device_.getQueueFamilyProperties();
      auto indexed_queue_family_props = std::views::enumerate(queue_family_props);

      // Try to find a queue family that supports both presentation and graphics families.
      auto queue_family_prop_it =
          std::ranges::find_if(indexed_queue_family_props, [&pd = physical_device_, &surf = surface_](auto&& pair) {
            auto&& [index, prop] = pair;
            return ((prop.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0)) &&
                   pd.getSurfaceSupportKHR(static_cast<uint32_t>(index), surf);
          });

      if (queue_family_prop_it == indexed_queue_family_props.end()) {
        // There is no a queue that supports both graphics and presentation queue families. We need separate queue
        // family.
        auto graphics_queue_family_prop_it = std::ranges::find_if(queue_family_props, [](const auto& prop) {
          return (prop.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
        });

        if (graphics_queue_family_prop_it == queue_family_props.end()) {
          eray::util::Logger::err("Could not find a graphics queue family on the physical device");
          std::unexpected(VulkanUnsupportedQueueFamily{.queue_family_name = "Graphics"});
        }

        graphics_queue_family_index_ =
            static_cast<uint32_t>(std::distance(queue_family_props.begin(), graphics_queue_family_prop_it));

        auto surface_queue_family_prop_it =
            std::ranges::find_if(indexed_queue_family_props, [&pd = physical_device_, &surf = surface_](auto&& pair) {
              auto&& [index, prop] = pair;
              return pd.getSurfaceSupportKHR(static_cast<uint32_t>(index), surf);
            });

        if (surface_queue_family_prop_it == indexed_queue_family_props.end()) {
          eray::util::Logger::err("Could not find a presentation queue family on the physical device");
          std::unexpected(VulkanUnsupportedQueueFamily{.queue_family_name = "Presentation"});
        }

        present_queue_family_index_ =
            static_cast<uint32_t>(std::distance(indexed_queue_family_props.begin(), surface_queue_family_prop_it));
      } else {
        graphics_queue_family_index_ = present_queue_family_index_ =
            static_cast<uint32_t>(std::distance(indexed_queue_family_props.begin(), queue_family_prop_it));
      }
    }

    float queue_priority          = 0.F;
    auto device_queue_create_info = vk::DeviceQueueCreateInfo{
        .queueFamilyIndex = graphics_queue_family_index_,
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

    // == 4. Queues Creation ===========================================================================================

    if (auto result = device_.getQueue(graphics_queue_family_index_, 0)) {
      graphics_queue_ = std::move(*result);
    } else {
      eray::util::Logger::err("Failed to create a graphics queue. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanQueueCreationFailure{.result = result.error()});
    }

    if (auto result = device_.getQueue(present_queue_family_index_, 0)) {
      present_queue_ = std::move(*result);
    } else {
      eray::util::Logger::err("Failed to create a presentation queue. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanQueueCreationFailure{.result = result.error()});
    }

    eray::util::Logger::succ("Successfully created a logical device and queues.");

    return {};
  }

  std::expected<void, VulkanInitError> create_surface() {
    VkSurfaceKHR surface{};
    if (glfwCreateWindowSurface(*instance_, window_, nullptr, &surface)) {
      return std::unexpected(VulkanSurfaceCreationFailure{});
    }
    surface_ = vk::raii::SurfaceKHR(instance_, surface);

    return {};
  }

  std::expected<void, VulkanInitError> create_swap_chain() {
    // Surface formats (pixel format, e.g. B8G8R8A8, color space e.g. SRGB)
    auto available_formats       = physical_device_.getSurfaceFormatsKHR(surface_);
    auto available_present_modes = physical_device_.getSurfacePresentModesKHR(surface_);

    if (available_formats.empty() || available_present_modes.empty()) {
      eray::util::Logger::info(
          "The physical device's swap chain support is not sufficient. Required at least one available format and at "
          "least one presentation mode.");
      return std::unexpected(VulkanSwapChainSupportIsNotSufficient{});
    }

    auto swap_surface_format = choose_swap_surface_format(available_formats);

    // Presentation mode represents the actual conditions for showing imgaes to the screen:
    //
    //  - VK_PRESENT_MODE_IMMEDIATE_KHR:    images are transferred to the screen right away -- tearing
    //
    //  - VK_PRESENT_MODE_FIFO_KHR:         swap chain uses FIFO queue, if the queue is full the program waits -- VSync
    //
    //  - VK_PRESENT_MODE_FIFO_RELAXED_KHR: similar to the previous one, if the app is late and the queue was empty, the
    //                                      image is send right away
    //
    //  - VK_PRESENT_MODE_MAILBOX_KHR:      another variant of the second mode., if the queue is full, instead of
    //                                      blocking, the images that are already queued are replaced with the new ones
    //                                      fewer latency, avoids tearing issues -- triple buffering
    //
    // Note: Only the VK_PRESENT_MODE_MAILBOX_KHR is guaranteed to be available

    auto swap_present_mode = choose_swap_presentMode(available_present_modes);

    // Basic Surface capabilities (min/max number of images in the swap chain, min/max width and height of images)
    auto surface_capabilities = physical_device_.getSurfaceCapabilitiesKHR(surface_);

    // Swap extend is the resolution of the swap chain images, and it's almost always exactly equal to the resolution
    // of the window that we're drawing to in pixels.
    auto swap_extent = choose_swap_extent(surface_capabilities);

    // It is recommended to request at least one more image than the minimum
    auto min_img_count = std::max(3U, surface_capabilities.minImageCount + 1);
    if (surface_capabilities.maxImageCount > 0 && min_img_count > surface_capabilities.maxImageCount) {
      // 0 is a special value that means that there is no maximum

      min_img_count = surface_capabilities.maxImageCount;
    }

    auto swap_chain_info = vk::SwapchainCreateInfoKHR{
        // Almost always left as default
        .flags = vk::SwapchainCreateFlagsKHR(),

        // Window surface on which the swap chain will present images
        .surface = surface_,  //

        // Minimum number of images (image buffers). More images reduce the risk of waiting for the GPU to finish
        // rendering, which improves performance
        .minImageCount = min_img_count,  //

        .imageFormat     = swap_surface_format.format,      //
        .imageColorSpace = swap_surface_format.colorSpace,  //
        .imageExtent     = swap_extent,                     //

        // Number of layers each image consists of (unless stereoscopic 3D app is developed it should be 1)
        .imageArrayLayers = 1,  //

        // Kind of images used in the swap chain (it's a bitfield, you can e.g. attach depth and stencil buffers)
        // Also you can render images to a separate image and perform post-processing
        // (VK_IMAGE_USAGE_TRANSFER_DST_BIT).
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,  //

        // We can specify that a certain transform should be applied to images in the swap chain if it is supported,
        // for example 90-degree clockwise rotation or horizontal flip. We specify no transform by using
        // surface_capabilities.currentTransform.
        .preTransform = surface_capabilities.currentTransform,  //

        // Value indicating the alpha compositing mode to use when this surface is composited together with other
        // surfaces on certain window systems
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,  //

        .presentMode = swap_present_mode,  //

        // Applications should set this value to VK_TRUE if they do not expect to read back the content of
        // presentable images before presenting them or after reacquiring them, and if their fragment shaders do not
        // have any side effects that require them to run for all pixels in the presentable image.
        //
        // If the clipped is VK_TRUE, then that means that we don't care about the color of pixels that are
        // obscured, for example, because another window is in front of them => better performance.
        .clipped = vk::True,  //

        // In Vulkan, it's possible that your swap chain becomes invalid or unoptimized while your app is running,
        // e.g. when window gets resized. In that case the swap chain actually needs to be recreated from scratch.
        // IN SUCH A CASE THE SWAP CHAIN NEEDS TO BE RECREATED FROM SCRATCH, and a reference to the old one must be
        // specified here.
        .oldSwapchain = VK_NULL_HANDLE,
    };

    // We need to specify how to handle swap chain images that will be used across multiple queue families. That will be
    // the case if graphics and present queue families are different.
    uint32_t indices[] = {graphics_queue_family_index_, present_queue_family_index_};

    // There are 2 ways to handle image ownership for queues:
    //  - VK_SHARING_MODE_EXCLUSIVE: Images can be used across multiple queue families without explicit ownership
    //  transfers.
    //  - VK_SHARING_MODE_CONCURRENT: The image is owned by one queue family at a time, and ownership must be explicitly
    //  transferred before
    // using it in another queue family. The best performance.

    if (graphics_queue_family_index_ != present_queue_family_index_) {
      // Multiple queues -> VK_SHARING_MODE_CONCURRENT to avoid ownership transfers and simplify the code. We are paying
      // a performance cost here.
      swap_chain_info.imageSharingMode = vk::SharingMode::eConcurrent;

      // Specify queues that will share the image ownership
      swap_chain_info.queueFamilyIndexCount = 2;
      swap_chain_info.pQueueFamilyIndices   = indices;
    } else {
      // One queue -> VK_SHARING_MODE_EXCLUSIVE
      swap_chain_info.imageSharingMode = vk::SharingMode::eExclusive;

      // No need to specify which queues share the image ownership
      swap_chain_info.queueFamilyIndexCount = 0;
      swap_chain_info.pQueueFamilyIndices   = nullptr;
    }

    if (auto result = device_.createSwapchainKHR(swap_chain_info)) {
      swap_chain_ = std::move(*result);
    } else {
      eray::util::Logger::err("Failed to create a presentation queue: {}", vk::to_string(result.error()));
      return std::unexpected(VulkanSwapChainCreationFailure{.result = result.error()});
    }
    swap_chain_images_ = swap_chain_.getImages();
    swap_chain_format_ = swap_surface_format.format;
    swap_chain_extent_ = swap_extent;

    eray::util::Logger::succ("Successfully created a Vulkan Swap chain.");

    return {};
  }

  vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
    for (const auto& surf_format : available_formats) {
      if (surf_format.format == vk::Format::eB8G8R8A8Srgb &&
          surf_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return surf_format;
      }
    }

    eray::util::Logger::warn(
        "A format B8G8R8A8Srgb with color space SrgbNonlinear is not supported by the Surface. A random format will be "
        "used.");

    return available_formats[0];
  }

  vk::PresentModeKHR choose_swap_presentMode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
    auto mode_it = std::ranges::find_if(available_present_modes, [](const auto& mode) {
      return mode ==
             vk::PresentModeKHR::eMailbox;  // Note: good if energy usage is not a concern, avoid for mobile devices
    });

    if (mode_it != available_present_modes.end()) {
      return *mode_it;
    }

    return vk::PresentModeKHR::eFifo;
  }

  vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    }

    // Unfortunately, if you are using a high DPI display (like Apple’s Retina display), screen coordinates don’t
    // correspond to pixels. For that reason we use glfwGetFrameBufferSize to get size in pixels. (Note:
    // glfwGetWindowSize returns size in screen coordinates).
    int width{};
    int height{};
    glfwGetFramebufferSize(window_, &width, &height);

    return {
        .width  = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width,
                             capabilities.maxImageExtent.width),
        .height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height,
                             capabilities.maxImageExtent.height),
    };
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
  uint32_t graphics_queue_family_index_{};

  /**
   * @brief Any presentation command might be submitted to this queue.
   *
   */
  vk::raii::Queue present_queue_ = nullptr;
  uint32_t present_queue_family_index_{};

  /**
   * @brief Vulkan allows for off-screen rendering, as well as rendering to a surface that is being displayed in
   * any Windowing API. This concept applies to mobile too. The SurfaceKHR usage is platform-agnostic, however it's
   * creation is not.
   * - On Linux with Wayland you need "VK_KHR_wayland_surface" and on windows you need
   * "VK_KHR_win32_surface" instance extension. Luckily the GLFW's `glfwGetRequiredInstanceExtensions` properly
   * returns the platform specific Vulkan extensions.
   * - Each extension provides different platform-specific createInfo structures, e.g. `VkWin32SurfaceCreateInfoKHR`.
   * - GLFW provides `glfwCreateWindowSurface` handle platform-specific surface creation for us.
   *
   */
  vk::raii::SurfaceKHR surface_ = nullptr;

  /**
   * @brief Vulkan does not provide a "default framebuffuer". Hence it requires an infrastructure that will own the
   * buffers we will render to before we visualize them on the screen. This infrastructure is known as the swap chain.
   *
   * The swap is a queue of images that are waiting to be presented to the screen. The general purpose of the swap chain
   * is to synchronize the presentation of images with the refresh rate of teh screen.
   *
   */
  vk::raii::SwapchainKHR swap_chain_ = nullptr;

  /**
   * @brief Stores handles to the Swap chain images.
   *
   */
  std::vector<vk::Image> swap_chain_images_;

  vk::Format swap_chain_format_ = vk::Format::eUndefined;
  vk::Extent2D swap_chain_extent_{};

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
