#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <liberay/os/system.hpp>
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
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

struct GLFWWindowCreationFailure {};

struct VulkanExtensionNotSupported {
  std::string glfw_extension;
};
struct SomeOfTheRequestedVulkanLayersAreNotSupported {};
struct FailedToEnumeratePhysicalDevices {
  vk::Result result;
};
struct NoSuitablePhysicalDevicesFound {};
struct VulkanUnsupportedQueueFamily {
  std::string queue_family_name;
};

struct VulkanObjectCreationError {
  std::optional<vk::Result> result;
  std::string what() {
    if (result) {
      return std::format("Creation error: {}", vk::to_string(*result));
    }
    return "Uknown creation error";
  }
};

struct VulkanSwapChainSupportIsNotSufficient {};

struct FileDoesNotExistError {};

struct FileStreamOpenFailure {};

struct FileError {
  std::variant<FileDoesNotExistError, FileStreamOpenFailure> kind;
  std::filesystem::path path;
};

using VulkanInitError =
    std::variant<VulkanExtensionNotSupported, SomeOfTheRequestedVulkanLayersAreNotSupported,
                 FailedToEnumeratePhysicalDevices, NoSuitablePhysicalDevicesFound, VulkanUnsupportedQueueFamily,
                 VulkanSwapChainSupportIsNotSufficient, FileError, VulkanObjectCreationError>;
using AppError = std::variant<GLFWWindowCreationFailure, VulkanInitError>;

struct SwapchainRecreationFailure {};
struct SwapChainImageAcquireFailure {};

using DrawFrameError = std::variant<SwapchainRecreationFailure, SwapChainImageAcquireFailure>;

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
    TRY(create_vk_instance())
    eray::util::Logger::succ("Successfully created a Vulkan Instance.");
    TRY(setup_debug_messenger())
    eray::util::Logger::succ("Successfully created a Vulkan Debug Messenger.");
    TRY(create_surface())
    eray::util::Logger::succ("Successfully created a Vulkan Surface.");
    TRY(pick_physical_device())
    eray::util::Logger::succ("Successfully picked a physical device with name {}",
                             std::string_view(physical_device_.getProperties().deviceName));
    TRY(create_logical_device())
    eray::util::Logger::succ("Successfully created a Vulkan Logical Device and Queues.");
    TRY(create_swap_chain())
    eray::util::Logger::succ("Successfully created a Vulkan Swap chain.");
    TRY(create_image_views())
    eray::util::Logger::succ("Successfully created Vulkan Swap chain image views.");
    TRY(create_graphics_pipeline())
    eray::util::Logger::succ("Successfully created Vulkan Graphics Pipeline.");
    TRY(create_command_pool())
    eray::util::Logger::succ("Successfully created Vulkan Command Pool.");
    TRY(create_command_buffers())
    eray::util::Logger::succ("Successfully created Vulkan Command Buffers.");
    TRY(create_sync_objs())

    return {};
  }

  std::expected<void, GLFWWindowCreationFailure> initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    glfwSetErrorCallback([](int error_code, const char* description) {
      eray::util::Logger::err("GLFW Error #{0}: {1}", error_code, description);
    });

    window_ = glfwCreateWindow(kWinWidth, kWinHeight, "Vulkan", nullptr, nullptr);
    if (!window_) {
      return std::unexpected(GLFWWindowCreationFailure{});
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebuffer_resize_callback);

    eray::util::Logger::succ("Successfully created a GLFW Window");

    return {};
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
      if (!draw_frame()) {
        eray::util::Logger::err("Closing window: Failed to draw a frame");
        break;
      }
    }

    // Since draw frame operations are async, when the main loop ends the drawing operations may still be going on.
    // This call is allows for the async operations to finish before cleaning the resources.
    device_.waitIdle();
  }

  std::expected<void, DrawFrameError> draw_frame() {
    // A binary (there is also a timeline semaphore) semaphore is used to add order between queue operations (work
    // submitted to the queue). Semaphores are used for both -- to order work inside the same queue and between
    // different queues. The waiting happens on GPU only, the host (CPU) is not blocked.
    //
    // A fence is used on CPU. Unlike the semaphores the vkWaitForFence is blocking the host.

    while (vk::Result::eTimeout == device_.waitForFences(*in_flight_fences_[current_frame_], vk::True, UINT64_MAX)) {
      ;
    }

    // Get the image from the swap chain. When the image will be ready the present semaphore will be signaled.
    auto [result, image_index] =
        swap_chain_.acquireNextImage(UINT64_MAX, *present_complete_semaphores_[current_semaphore_], nullptr);

    if (result == vk::Result::eErrorOutOfDateKHR) {
      // The swap chain has become incompatible with the surface and can no longer be used for rendering. Usually
      // happens after window resize.
      TRY(recreate_swap_chain());
      return {};
    }

    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      // The swap chain cannot be used even if we accept that the surface properties are no longer matched exactly
      // (eSuboptimalKHR).
      eray::util::Logger::err("Failed to present swap chain image");
      return std::unexpected(SwapChainImageAcquireFailure{});
    }

    device_.resetFences(*in_flight_fences_[current_frame_]);
    command_buffers_[current_frame_].reset();
    record_command_buffer(current_frame_, image_index);

    auto wait_dst_stage_mask = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const auto submit_info   = vk::SubmitInfo{
          .waitSemaphoreCount   = 1,
          .pWaitSemaphores      = &*present_complete_semaphores_[current_semaphore_],
          .pWaitDstStageMask    = &wait_dst_stage_mask,
          .commandBufferCount   = 1,
          .pCommandBuffers      = &*command_buffers_[current_frame_],
          .signalSemaphoreCount = 1,
          .pSignalSemaphores    = &*render_finished_semaphores_[image_index],  //
    };

    // Submits the provided commands to the queue. The vkWaitForFences blocks the execution until all
    // of the commands will be submitted. The submitting will begin after the present semaphore
    // receives the signal from acquire next image.
    //
    // When the rendering finishes, the finished render finished semaphore is signaled.
    //
    graphics_queue_.submit(submit_info, *in_flight_fences_[current_frame_]);

    // The image will not be presented until the render finished semaphore is signaled by the submit call.
    const auto present_info = vk::PresentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &*render_finished_semaphores_[image_index],
        .swapchainCount     = 1,
        .pSwapchains        = &*swap_chain_,
        .pImageIndices      = &image_index,
        .pResults           = nullptr,
    };

    result = present_queue_.presentKHR(present_info);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebuffer_resized_) {
      framebuffer_resized_ = false;
      TRY(recreate_swap_chain());
    } else if (result != vk::Result::eSuccess) {
      eray::util::Logger::err("Failed to present swap chain image");
      return std::unexpected(SwapChainImageAcquireFailure{});
    }

    current_semaphore_ = (current_semaphore_ + 1) % present_complete_semaphores_.size();
    current_frame_     = (current_frame_ + 1) % kMaxFramesInFlight;

    return {};
  }

  void cleanup_swapchain() {
    // Swap chain must be destroyed before destroying the GLFW window, getDispatcher()->vkDestroySwapchainKHR throws
    // otherwise
    swap_chain_image_views_.clear();
    swap_chain_ = nullptr;
  }

  void cleanup() {
    cleanup_swapchain();

    glfwDestroyWindow(window_);
    glfwTerminate();

    eray::util::Logger::succ("Finished cleanup");
  }

  std::expected<void, VulkanInitError> create_vk_instance() {
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
      return std::unexpected(VulkanObjectCreationError{
          .result = instance_opt.error(),  //
      });
    }

    eray::util::Logger::succ("Successfully created a Vulkan Instance");

    return {};
  }

  std::expected<void, VulkanObjectCreationError> setup_debug_messenger() {
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
      return std::unexpected(VulkanObjectCreationError{
          .result = debug_messenger_opt.error(),  //
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
        {},                                                            // vk::PhysicalDeviceFeatures2
        {.synchronization2 = vk::True, .dynamicRendering = vk::True},  // Enable dynamic rendering from Vulkan 1.3
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
      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }

    // == 4. Queues Creation ===========================================================================================

    if (auto result = device_.getQueue(graphics_queue_family_index_, 0)) {
      graphics_queue_ = std::move(*result);
    } else {
      eray::util::Logger::err("Failed to create a graphics queue. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }

    if (auto result = device_.getQueue(present_queue_family_index_, 0)) {
      present_queue_ = std::move(*result);
    } else {
      eray::util::Logger::err("Failed to create a presentation queue. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }

    return {};
  }

  std::expected<void, VulkanInitError> create_surface() {
    VkSurfaceKHR surface{};
    if (glfwCreateWindowSurface(*instance_, window_, nullptr, &surface)) {
      return std::unexpected(VulkanObjectCreationError{
          .result = std::nullopt,
      });
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
        // e.g. when window gets resized. IN SUCH A CASE THE SWAP CHAIN NEEDS TO BE RECREATED FROM SCRATCH, and a
        // reference to the old one must be specified here.
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
      eray::util::Logger::err("Failed to create a swap chain: {}", vk::to_string(result.error()));
      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }
    swap_chain_images_ = swap_chain_.getImages();
    swap_chain_format_ = swap_surface_format.format;
    swap_chain_extent_ = swap_extent;

    return {};
  }

  std::expected<void, SwapchainRecreationFailure> recreate_swap_chain() {
    int width  = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window_, &width, &height);
      glfwWaitEvents();
    }

    device_.waitIdle();

    cleanup_swapchain();
    if (!create_swap_chain()) {
      eray::util::Logger::err("Could not recreate a swap chain: Swap chain creation failed.");

      return std::unexpected(SwapchainRecreationFailure{});
    }

    if (!create_image_views()) {
      eray::util::Logger::err("Could not recreate a swap chain: Image views creation failed.");

      return std::unexpected(SwapchainRecreationFailure{});
    }

    return {};
  }

  std::expected<void, VulkanInitError> create_image_views() {
    auto image_view_info =
        vk::ImageViewCreateInfo{.viewType = vk::ImageViewType::e2D,
                                .format   = swap_chain_format_,

                                // You can map some channels onto the others. We stick to defaults here.
                                .components =
                                    {
                                        .r = vk::ComponentSwizzle::eIdentity,
                                        .g = vk::ComponentSwizzle::eIdentity,
                                        .b = vk::ComponentSwizzle::eIdentity,
                                        .a = vk::ComponentSwizzle::eIdentity,
                                    },

                                // Describes what the image's purpose is and which part of the image should be accessed.
                                // The images here will be used as color targets with no mipmapping levels and
                                // without any multiple layers
                                .subresourceRange = vk::ImageSubresourceRange{
                                    .aspectMask     = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel   = 0,
                                    .levelCount     = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount     = 1  //
                                }};

    for (auto image : swap_chain_images_) {
      image_view_info.image = image;
      if (auto result = device_.createImageView(image_view_info)) {
        swap_chain_image_views_.emplace_back(std::move(*result));
      } else {
        eray::util::Logger::err("Failed to create a swap chain image view: {}", vk::to_string(result.error()));
        return std::unexpected(VulkanObjectCreationError{
            .result = result.error(),
        });
      }
    }

    return {};
  }

  std::expected<void, VulkanInitError> create_graphics_pipeline() {
    auto common_err = [](auto&& err) -> VulkanInitError { return std::forward<decltype(err)>(err); };

    // == 1. Shader stage ==============================================================================================

    auto shader_module_opt = read_binary(eray::os::System::executable_dir() / "shaders" / "main_sh.spv")
                                 .transform_error(common_err)
                                 .and_then([this, common_err](const auto& bytecode) {
                                   return create_shader_module(bytecode).transform_error(common_err);
                                 });

    if (!shader_module_opt) {
      return std::unexpected(std::move(shader_module_opt.error()));
    }
    auto shader_module = std::move(*shader_module_opt);

    auto vert_shader_stage_pipeline_info = vk::PipelineShaderStageCreateInfo{
        .stage  = vk::ShaderStageFlagBits::eVertex,  //
        .module = shader_module,                     //
        .pName  = kVertexShaderEntryPoint.c_str(),   // entry point name

        // Optional: pSpecializationInfo allows to specify values for shader constants. This allows for compiler
        // optimizations like eliminating if statements that depend on the const values.
    };

    auto frag_shader_stage_pipeline_info = vk::PipelineShaderStageCreateInfo{
        .stage  = vk::ShaderStageFlagBits::eFragment,
        .module = shader_module,
        .pName  = kFragmentShaderEntryPoint.c_str(),
    };

    auto shader_stages = std::array<vk::PipelineShaderStageCreateInfo, 2>{vert_shader_stage_pipeline_info,
                                                                          frag_shader_stage_pipeline_info};

    // == 2. Dynamic state =============================================================================================

    // Most of the pipeline state needs to be baked into the pipeline state. For example changing the size of a
    // viewport, line width and blend constants can be changed dynamically without the full pipeline recreation.
    //
    // Note: This will cause the configuration of these values to be ignored, and you will be able (and required)
    // to specify the data at drawing time.
    auto dynamic_states = std::vector{
        vk::DynamicState::eViewport,  //
        vk::DynamicState::eScissor    //
    };

    auto dynamic_state = vk::PipelineDynamicStateCreateInfo{
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),  //
        .pDynamicStates    = dynamic_states.data(),                         //
    };

    // With dynamic state only the count is necessary.
    auto viewport_state_info = vk::PipelineViewportStateCreateInfo{.viewportCount = 1, .scissorCount = 1};

    // == 3. Input assembly ============================================================================================

    // Describes the format of the vertex data that will be passed to the vertex shader:
    // - Bindings: spacing between data and whether the data is per-vertex or per-instance,
    // - Attribute descriptions: type of the attributes passed to the vertex shader, which binding to load them from and
    // at which offset
    auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo{
        // There is no input passed yet
    };

    // Describes:
    // - what kind of geometry will be drawn
    //   VK_PRIMITIVE_TOPOLOGY_(POINT_LIST|LINE_LIST|LINE_STRIP|TRIANGLE_LIST|TRIANGLE_STRIP)
    // - whether primitive restart should be enabled, when set to VK_TRUE, it's possible to break up lines and triangles
    //   in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF
    auto input_assembly = vk::PipelineInputAssemblyStateCreateInfo{
        .topology               = vk::PrimitiveTopology::eTriangleList,  //
        .primitiveRestartEnable = vk::False,                             //
    };

    // == 4. Rasterizer ================================================================================================

    // The Rasterizer takes as it's input geometry and turns it into fragments to be colored by the fragment shader.
    // It also performs face culling, depth testing and the scissor test. It also allows for wireframe rendering.

    auto rasterization_state_info = vk::PipelineRasterizationStateCreateInfo{
        .depthClampEnable =
            vk::False,  // whether fragment depths should be clamped to [minDepth, maxDepth] (to near and far planes)
        .polygonMode = vk::PolygonMode::eFill,  // you can use eLine for wireframes
        .cullMode    = vk::CullModeFlagBits::eBack,
        .frontFace   = vk::FrontFace::eClockwise,

        // Polygons that are coplanar in 3D space can be made to appear as if they are not coplanar by adding a z-bias
        // (or depth bias) to each one. This is a technique commonly used to ensure that shadows in a scene are
        // displayed properly. For instance, a shadow on a wall will likely have the same depth value as the wall. If an
        // application renders a wall first and then a shadow, the shadow might not be visible, or depth artifacts might
        // be visible.
        .depthBiasEnable      = vk::False,
        .depthBiasSlopeFactor = 1.0F,

        // NOTE: The maximum line width that is supported dependson the hardware and any lin thicker
        // than 1.0F requires to enable the wideLines GPU feature.
        .lineWidth = 1.0F,
    };

    // == 5. Multisampling =============================================================================================

    auto multisampling_state_info = vk::PipelineMultisampleStateCreateInfo{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable  = vk::False,
    };

    // == 6. Depth and Stencil Testing =================================================================================

    // TODO(migoox): Add

    // == 7. Color blending ============================================================================================

    auto color_blend_attachment = vk::PipelineColorBlendAttachmentState{
        .blendEnable = vk::True,

        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
        .colorBlendOp        = vk::BlendOp::eAdd,  //

        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eZero,
        .alphaBlendOp        = vk::BlendOp::eAdd,

        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,  //
    };

    auto color_blending_info = vk::PipelineColorBlendStateCreateInfo{
        .logicOpEnable   = vk::False,
        .logicOp         = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments    = &color_blend_attachment,  //
    };

    // == 8. Pipeline Layout creation ==================================================================================

    // You can use uniform values in shaders, which are globals that can be changed at drawing time after the behavior
    // of your shaders without having to recreate. The uniform variables must be specified during the pipeline creation.
    auto pipeline_layout_info = vk::PipelineLayoutCreateInfo{
        .setLayoutCount         = 0,
        .pushConstantRangeCount = 0  //
    };

    if (auto result = device_.createPipelineLayout(pipeline_layout_info)) {
      pipeline_layout_ = std::move(*result);
    } else {
      eray::util::Logger::info("Could not create a pipeline layout. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }

    // == 9. Graphics Pipeline  ========================================================================================

    // We use the dynamic rendering feature (Vulkan 1.3), the structure below specifies color attachment data, and
    // the format. In previous versions of Vulkan, we would need to create framebuffers to bind our image views to
    // a render pass, so the dynamic rendering eliminates the need for render pass and framebuffer.
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info{
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &swap_chain_format_,
    };

    auto pipeline_info = vk::GraphicsPipelineCreateInfo{
        .pNext = &pipeline_rendering_create_info,  //

        .stageCount = shader_stages.size(),  //
        .pStages    = shader_stages.data(),  //

        .pVertexInputState   = &vertex_input_state,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport_state_info,
        .pRasterizationState = &rasterization_state_info,
        .pMultisampleState   = &multisampling_state_info,
        .pColorBlendState    = &color_blending_info,
        .pDynamicState       = &dynamic_state,
        .layout              = pipeline_layout_,

        .renderPass = nullptr,  // we are using dynamic rendering

        // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex  = -1,
    };

    // Pipeline cache (set to nullptr) can be used to store and reuse data relevant to pipeline creation across
    // multiple calls to vk::CreateGraphicsPipelines and even across program executions if the cache is stored to a
    // file.
    if (auto result = device_.createGraphicsPipeline(nullptr, pipeline_info)) {
      graphics_pipeline_ = std::move(*result);
    } else {
      eray::util::Logger::err("Could not create a graphics pipeline. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }

    return {};
  }

  std::expected<void, VulkanInitError> create_command_pool() {
    auto command_pool_info = vk::CommandPoolCreateInfo{
        // There are two possible flags for command pools:
        // - VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often
        //   (may change memory allocation behavior).
        // - VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually,
        //   without this flag they all have to be reset together. (Reset and rerecord over it in every frame)

        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,

        // Each command pool can only allocate command buffers that are submitted on a single type of queue.
        // We setup commands for drawing, and thus we've chosen the graphics queue family.
        .queueFamilyIndex = graphics_queue_family_index_,
    };

    if (auto result = device_.createCommandPool(command_pool_info)) {
      command_pool_ = std::move(*result);
    } else {
      eray::util::Logger::err("Could not create a command pool. {}", vk::to_string(result.error()));

      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }

    return {};
  }

  std::expected<void, VulkanInitError> create_command_buffers() {
    auto alloc_info = vk::CommandBufferAllocateInfo{
        .commandPool = command_pool_,  //

        // Specifies if the allocated command buffers are primary or secondary command buffers:
        // - VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from
        // other
        //   command buffers.
        // - VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command
        //   buffers.
        .level              = vk::CommandBufferLevel::ePrimary,  //
        .commandBufferCount = kMaxFramesInFlight,                //
    };

    if (auto result = device_.allocateCommandBuffers(alloc_info)) {
      std::ranges::move(*result | std::views::take(kMaxFramesInFlight), command_buffers_.begin());
    } else {
      eray::util::Logger::err("Command buffer allocation failure. {}", vk::to_string(result.error()));
      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }

    return {};
  }

  std::expected<void, VulkanInitError> create_sync_objs() {
    present_complete_semaphores_.clear();
    render_finished_semaphores_.clear();

    for (size_t i = 0; i < swap_chain_images_.size(); ++i) {
      if (auto result = device_.createSemaphore(vk::SemaphoreCreateInfo{})) {
        present_complete_semaphores_.emplace_back(std::move(*result));
      } else {
        eray::util::Logger::err("Failed to create a  sempahore. {}", vk::to_string(result.error()));
        return std::unexpected(VulkanObjectCreationError{.result = result.error()});
      }
    }

    for (size_t i = 0; i < swap_chain_images_.size(); ++i) {
      if (auto result = device_.createSemaphore(vk::SemaphoreCreateInfo{})) {
        render_finished_semaphores_.emplace_back(std::move(*result));
      } else {
        eray::util::Logger::err("Failed to create a sempahore. {}", vk::to_string(result.error()));
        return std::unexpected(VulkanObjectCreationError{.result = result.error()});
      }
    }

    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
      if (auto result = device_.createFence(vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled})) {
        in_flight_fences_[i] = std::move(*result);
      } else {
        eray::util::Logger::err("Failed to create a fence. {}", vk::to_string(result.error()));
        return std::unexpected(VulkanObjectCreationError{.result = result.error()});
      }
    }

    return {};
  }

  struct TransitionImageLayoutInfo {
    uint32_t image_index;
    size_t frame_index;
    vk::ImageLayout old_layout;
    vk::ImageLayout new_layout;
    vk::AccessFlags2 src_access_mask;
    vk::AccessFlags2 dst_access_mask;
    vk::PipelineStageFlags2 src_stage_mask;
    vk::PipelineStageFlags2 dst_stage_mask;
  };

  /**
   * @brief In Vulkan, images can be in different layouts that are optimized for different operations. For example, an
   * image can be in a layout that is optimal for presenting to the screen, or in a layout that is optimal for being
   * used as a color attachment.
   *
   * This function is used to transition the image layout before and after rendering.
   *
   * @param image_index
   * @param old_layout
   * @param new_layout
   * @param src_access_mask
   * @param dst_access_mask
   * @param src_stage_mask
   * @param dst_stage_mask
   */
  void transition_image_layout(TransitionImageLayoutInfo info) {
    auto barrier = vk::ImageMemoryBarrier2{
        .srcStageMask        = info.src_stage_mask,
        .srcAccessMask       = info.src_access_mask,
        .dstStageMask        = info.dst_stage_mask,
        .dstAccessMask       = info.dst_access_mask,
        .oldLayout           = info.old_layout,
        .newLayout           = info.new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swap_chain_images_[info.image_index],  //
        .subresourceRange =
            vk::ImageSubresourceRange{
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
    };

    auto dependency_info = vk::DependencyInfo{
        .dependencyFlags         = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };

    command_buffers_[info.frame_index].pipelineBarrier2(dependency_info);
  }

  /**
   * @brief Writes the commands we what to execute into a command buffer
   *
   * @param image_index
   */
  void record_command_buffer(size_t frame_index, uint32_t image_index) {
    command_buffers_[frame_index].begin(vk::CommandBufferBeginInfo{
        // The `flags` parameter specifies how we're going to use the command buffer:
        // - VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing
        // it
        //   once.
        // - VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be
        // entirely
        //   within a single render pass.
        // - WK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also
        //   already pending execution.
    });

    // Transition the image layout from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
    transition_image_layout(TransitionImageLayoutInfo{
        .image_index     = image_index,
        .frame_index     = frame_index,
        .old_layout      = vk::ImageLayout::eUndefined,
        .new_layout      = vk::ImageLayout::eColorAttachmentOptimal,
        .src_access_mask = {},
        .dst_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .src_stage_mask  = vk::PipelineStageFlagBits2::eTopOfPipe,
        .dst_stage_mask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    });

    // Set up the color attachment
    vk::ClearValue clear_color = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F);
    auto attachment_info       = vk::RenderingAttachmentInfo{

        // Specifies which image to render to
        .imageView = swap_chain_image_views_[image_index],

        // Specifies the layout the image will be in during rendering
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,

        // Specifies what to do with the image before rendering
        .loadOp = vk::AttachmentLoadOp::eClear,

        // Specifies what to do with the image after rendering
        .storeOp = vk::AttachmentStoreOp::eStore,

        .clearValue = clear_color,
    };

    auto rendering_info = vk::RenderingInfo{
        // Defines the size of the render area
        .renderArea =
            vk::Rect2D{
                .offset = {.x = 0, .y = 0}, .extent = swap_chain_extent_  //
            },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &attachment_info,
    };
    command_buffers_[frame_index].beginRendering(rendering_info);

    // We can specify type of the pipeline
    command_buffers_[frame_index].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    // Describes the region of framebuffer that the output will be rendered to
    command_buffers_[frame_index].setViewport(
        0, vk::Viewport{
               .x      = 0.0F,
               .y      = 0.0F,
               .width  = static_cast<float>(swap_chain_extent_.width),
               .height = static_cast<float>(swap_chain_extent_.height),
               // Note: min and max depth must be between [0.0F, 1.0F] and min might be higher than max.
               .minDepth = 0.0F,
               .maxDepth = 1.0F  //
           });

    // Scissor rectangle defines in which region pixels will actually be stored. The rasterizer will discard any
    // pixels outside the scissored rectangle. We want to draw to entire framebuffer.
    command_buffers_[frame_index].setScissor(
        0, vk::Rect2D{.offset = vk::Offset2D{.x = 0, .y = 0}, .extent = swap_chain_extent_});

    // Draw 3 vertices
    command_buffers_[frame_index].draw(3, 1, 0, 0);

    command_buffers_[frame_index].endRendering();

    // Transition the image layout from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
    transition_image_layout(TransitionImageLayoutInfo{
        .image_index     = image_index,
        .frame_index     = frame_index,
        .old_layout      = vk::ImageLayout::eColorAttachmentOptimal,
        .new_layout      = vk::ImageLayout::ePresentSrcKHR,
        .src_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dst_access_mask = {},
        .src_stage_mask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dst_stage_mask  = vk::PipelineStageFlagBits2::eBottomOfPipe,
    });

    command_buffers_[frame_index].end();
  }

  vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
    for (const auto& surf_format : available_formats) {
      if (surf_format.format == vk::Format::eB8G8R8A8Srgb &&
          surf_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
        return surf_format;
      }
    }

    eray::util::Logger::warn(
        "A format B8G8R8A8Srgb with color space SrgbNonlinear is not supported by the Surface. A random format will "
        "be "
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

  static void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
    auto* app                 = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
    app->framebuffer_resized_ = true;
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

  static std::expected<std::vector<char>, FileError> read_binary(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
      eray::util::Logger::err("File {} does not exist", path.string());
      return std::unexpected(FileError{.kind = FileDoesNotExistError{}, .path = path});
    }
    auto file = std::ifstream(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
      eray::util::Logger::err("Unable to open a stream for file {}", path.string());
      return std::unexpected(FileError{.kind = FileStreamOpenFailure{}, .path = path});
    }

    auto bytes  = static_cast<size_t>(file.tellg());
    auto buffer = std::vector<char>(bytes, 0);
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    if (file.bad()) {
      eray::util::Logger::warn("File {} was not closed properly", path.string());
    }

    eray::util::Logger::info("Read {} bytes from {}", bytes, path.string());

    return buffer;
  }

  [[nodiscard]] std::expected<vk::raii::ShaderModule, VulkanObjectCreationError> create_shader_module(
      const std::vector<char>& bytecode) {
    // The size of the bytecode is specified in bytes, but the bytecode pointer is a `uint32_t` pointer. The data is
    // stored in an `std::vector` where the default allocator already ensures that the data satisfies the worst case
    // alignment requirements, so the data will satisfy the `uint32_t` alignment requirements.
    auto module_info = vk::ShaderModuleCreateInfo{
        .codeSize = bytecode.size() * sizeof(char),                     //
        .pCode    = reinterpret_cast<const uint32_t*>(bytecode.data())  //
    };

    // Shader modules are a thin wrapper around the shader bytecode.
    auto result = device_.createShaderModule(module_info);
    if (result) {
      return std::move(*result);
    }
    eray::util::Logger::err("Failed to create a shader module");
    return std::unexpected(VulkanObjectCreationError{
        .result = result.error(),
    });
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
   * The swap is a queue of images that are waiting to be presented to the screen. The general purpose of the swap
   * chain is to synchronize the presentation of images with the refresh rate of teh screen.
   *
   */
  vk::raii::SwapchainKHR swap_chain_ = nullptr;

  /**
   * @brief Stores handles to the Swap chain images.
   *
   */
  std::vector<vk::Image> swap_chain_images_;

  /**
   * @brief Describes the format e.g. RGBA.
   *
   */
  vk::Format swap_chain_format_ = vk::Format::eUndefined;

  /**
   * @brief Describes the dimensions of the swap chain.
   *
   */
  vk::Extent2D swap_chain_extent_{};

  /**
   * @brief An image view DESCRIBES HOW TO ACCESS THE IMAGE and which part of the image to access, for example, if it
   * should be treated as a 2D texture depth texture without any mipmapping levels.
   *
   */
  std::vector<vk::raii::ImageView> swap_chain_image_views_;

  /**
   * @brief Describes the uniform buffers used in shaders.
   *
   */
  vk::raii::PipelineLayout pipeline_layout_ = nullptr;

  /**
   * @brief Describes the graphics pipeline, including shaders tages, input assembly, rasterization and more.
   *
   */
  vk::raii::Pipeline graphics_pipeline_ = nullptr;

  /**
   * @brief Command pools manage the memory that is used to store the buffers and command buffers are allocated from
   * them.
   *
   */
  vk::raii::CommandPool command_pool_ = nullptr;

  uint32_t current_semaphore_ = 0;
  uint32_t current_frame_     = 0;

  // Multiple frames are created in flight at once. Rendering of one frame does not interfere with the recording of
  // the other. We choose the number 2, because we don't want the CPU to go to far ahead of the GPU.
  static constexpr int kMaxFramesInFlight = 2;

  /**
   * @brief Drawing operations are recorded in comand buffer objects.
   *
   */
  std::array<vk::raii::CommandBuffer, kMaxFramesInFlight> command_buffers_ = {nullptr, nullptr};

  /**
   * @brief Semaphores are used to assert on GPU that a process e.g. rendering is finished.
   *
   */
  std::vector<vk::raii::Semaphore> present_complete_semaphores_;
  std::vector<vk::raii::Semaphore> render_finished_semaphores_;

  /**
   * @brief Fences are used to block GPU until the frame is presented.
   *
   */
  std::array<vk::raii::Fence, kMaxFramesInFlight> in_flight_fences_ = {nullptr, nullptr};

  /**
   * @brief GLFW window pointer.
   *
   */
  GLFWwindow* window_ = nullptr;

  /**
   * @brief Although many drivers and platforms trigger VK_ERROR_OUT_OF_DATE_KHR automatically after a window resize, it
   * is not guaranteed to happen. That's why there is an extra code to handle resizes explicitly.
   *
   */
  bool framebuffer_resized_ = false;

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
  static constexpr std::array<const char*, 5> kDeviceExtensions = {
      vk::KHRSwapchainExtensionName,             // requires Surface Instance Extension
      vk::KHRSpirv14ExtensionName,               //
      vk::KHRShaderDrawParametersExtensionName,  // BaseInstance, BaseVertex, DrawIndex
      vk::KHRSynchronization2ExtensionName,      //
      vk::KHRCreateRenderpass2ExtensionName      //
  };

  static constexpr eray::util::zstring_view kVertexShaderEntryPoint   = "mainVert";
  static constexpr eray::util::zstring_view kFragmentShaderEntryPoint = "mainFrag";
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
