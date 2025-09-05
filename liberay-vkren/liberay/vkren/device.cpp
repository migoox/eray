#include <GLFW/glfw3.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <expected>
#include <liberay/os/window/glfw/glfw_window.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/try.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <map>
#include <ranges>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_profiles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

Device::CreateInfo Device::CreateInfo::DesktopProfile::get(const eray::os::Window& window) noexcept {
  if (window.window_api() != eray::os::WindowAPI::GLFW) {
    eray::util::panic("Renderer supports GLFW only, but {} has been provided",
                      os::kWindowingAPIName[window.window_api()]);
  }

  auto* window_handle = reinterpret_cast<GLFWwindow*>(window.win_handle());

  // == Global Extensions ============================================================================================
  auto required_global_extensions = std::vector<const char*>();
  {
    uint32_t glfw_extensions_count = 0;
    if (auto* glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count)) {
      required_global_extensions = std::vector<const char*>(glfw_extensions, glfw_extensions + glfw_extensions_count);
      eray::util::Logger::info("{}", required_global_extensions);
    } else {
      eray::util::panic("Could not get required instance extensions from GLFW");
    }
  }

  // == Surface Creator ==============================================================================================
  auto surf_creator = [window_handle](const vk::raii::Instance& instance) -> std::optional<vk::raii::SurfaceKHR> {
    VkSurfaceKHR surface{};
    if (glfwCreateWindowSurface(*instance, window_handle, nullptr, &surface)) {
      eray::util::Logger::info("Could not create a window surface");
      return std::nullopt;
    }

    return vk::raii::SurfaceKHR(instance, surface);
  };

  return get(surf_creator, required_global_extensions);
}

Device::CreateInfo Device::CreateInfo::DesktopProfile::get(
    const SurfaceCreator& surface_creator_func, std::span<const char* const> required_global_extensions) noexcept {
  // == Validation Layers ==============================================================================================
  validation_layers_.clear();
#ifndef NDEBUG
  validation_layers_.emplace_back("VK_LAYER_KHRONOS_validation");
#endif

  // == Global Extensions ==============================================================================================
  global_extensions_.clear();
  global_extensions_.resize(required_global_extensions.size());
  std::ranges::copy(required_global_extensions.begin(), required_global_extensions.end(), global_extensions_.begin());
  // Note: vk::EXTDebugUtilsExtensionName is added automatically if validation layers are not empty

  // == Device Extensions ==============================================================================================
  device_extensions_.clear();
  device_extensions_ = {
      vk::KHRSwapchainExtensionName,         // requires Surface Instance Extension
      vk::KHRSpirv14ExtensionName,           //
      vk::KHRSynchronization2ExtensionName,  //
      vk::KHRCreateRenderpass2ExtensionName  //
  };

  // feature chains are defined by the profile only

  // == App info =======================================================================================================
  auto app_create_info = vk::ApplicationInfo{
      .pApplicationName   = "Vulkan Application",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),  // NOLINT
      .pEngineName        = "No Engine",
      .engineVersion      = VK_MAKE_VERSION(1, 0, 0),  // NOLINT
      .apiVersion         = vk::ApiVersion14           //
  };

  return CreateInfo{
      .profile_properties = {VP_KHR_ROADMAP_2022_NAME, VP_KHR_ROADMAP_2022_SPEC_VERSION},
      .validation_layers  = validation_layers_,
      .global_extensions  = global_extensions_,
      .device_extensions  = device_extensions_,
      .surface_creator    = surface_creator_func,
      .severity_flags =
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
      .app_info = std::move(app_create_info),
  };
}

Result<Device, Error> Device::create(vk::raii::Context& ctx, const CreateInfo& info) noexcept {
  auto device = Device();
  TRY(device.create_instance(ctx, info));
  TRY(device.create_debug_messenger(info));
  if (auto result = info.surface_creator(device.instance())) {
    device.surface_ = std::move(*result);
  } else {
    util::Logger::err("Failed to create a surface with the injected surface creator.");
    return std::unexpected(Error{
        .msg  = "Surface creation failure",
        .code = ErrorCode::SurfaceCreationFailure{},
    });
  }
  TRY(device.pick_physical_device(info));
  TRY(device.create_logical_device(info));
  TRY(device.create_command_pool());

  auto allocator_info           = VmaAllocatorCreateInfo{};
  allocator_info.physicalDevice = *device.physical_device_;
  allocator_info.device         = *device.device_;
  allocator_info.instance       = *device.instance_;
  allocator_info.flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  vmaCreateAllocator(&allocator_info, &device.allocator_);
  device.main_deletion_queue_.push_deletor([&]() { vmaDestroyAllocator(device.allocator_); });

  return device;
}

Result<void, Error> Device::create_instance(vk::raii::Context& ctx, const CreateInfo& info) noexcept {
  // == Vulkan Profiles ================================================================================================
  auto supported = vk::False;
  vpGetInstanceProfileSupport(nullptr, &info.profile_properties, &supported);
  if (!supported) {
    return std::unexpected(Error{
        .msg = "KHR_ROADMAP_2022 is required but not supported",
        .code =
            ErrorCode::ProfileNotSupported{
                .name    = std::string(info.profile_properties.profileName),
                .version = info.profile_properties.specVersion,
            },
    });
  }

  // == Additional extensions ==========================================================================================

  // Check if the requested extensions are supported by the Vulkan implementation.
  auto extensions_props = ctx.enumerateInstanceExtensionProperties();
  auto glob_extensions  = global_extensions(info);
  for (const auto& ext : glob_extensions) {
    if (std::ranges::none_of(extensions_props, [ext_name = std::string_view(ext)](const auto& prop) {
          return std::string_view(prop.extensionName) == ext_name;
        })) {
      eray::util::Logger::err("Failed to create a Vulkan Instance. Requested extension {} is not supported", ext);
      return std::unexpected(Error{
          .msg  = std::format("Extension {} is not supported", ext),
          .code = ErrorCode::ExtensionNotSupported{.extension = ext},
      });
    }
  }

  // == Validation layers ==============================================================================================

  // Check if the requested validation layers are supported by the Vulkan implementation.
  auto layer_props = ctx.enumerateInstanceLayerProperties();
  if (std::ranges::any_of(info.validation_layers, [&layer_props](const auto& required_layer) {
        return std::ranges::none_of(layer_props, [required_layer](auto const& layer_prop) {
          return required_layer == std::string_view(layer_prop.layerName);
        });
      })) {
    eray::util::Logger::err("Failed to create a Vulkan Instance. Use of unsupported validation layer(s).");
    return std::unexpected(Error{
        .msg  = "Unsupported validation layer(s) provided",
        .code = ErrorCode::ValidationLayerNotSupported{},
    });
  }

  // == Instance Creation ==============================================================================================
  auto instance_info = vk::InstanceCreateInfo{
      .pApplicationInfo        = &info.app_info,
      .enabledLayerCount       = static_cast<uint32_t>(info.validation_layers.size()),
      .ppEnabledLayerNames     = info.validation_layers.data(),
      .enabledExtensionCount   = static_cast<uint32_t>(glob_extensions.size()),
      .ppEnabledExtensionNames = glob_extensions.data(),
  };

  auto vp_instance_info                    = VpInstanceCreateInfo{};
  vp_instance_info.pCreateInfo             = reinterpret_cast<const VkInstanceCreateInfo*>(&instance_info);
  vp_instance_info.enabledFullProfileCount = 1;
  vp_instance_info.pEnabledFullProfiles    = &info.profile_properties;

  VkInstance vp_instance = nullptr;
  if (auto result = vk::Result(vpCreateInstance(&vp_instance_info, nullptr, &vp_instance));
      result != vk::Result::eSuccess) {
    eray::util::Logger::err("Failed to create a Vulkan Instance. Error type: {}", vk::to_string(result));
    return std::unexpected(Error{
        .msg     = "Vulkan Instance Creation error",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = result,
    });
  }
  auto instance = vk::Instance{vp_instance};
  instance_     = vk::raii::Instance{ctx, instance};
  eray::util::Logger::succ("Successfully created a Vulkan Instance");

  return {};
}

Result<void, Error> Device::create_debug_messenger(const CreateInfo& info) noexcept {
  if (info.validation_layers.empty()) {
    eray::util::Logger::info("Debug messenger setup omitted: No validation layers provided.");
    return {};
  }
  auto severity_flags = vk::DebugUtilsMessageSeverityFlagsEXT(info.severity_flags);
  auto msg_type_flags = vk::DebugUtilsMessageTypeFlagsEXT(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                          vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                          vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

  auto debug_utils_messenger_create_info_ext = vk::DebugUtilsMessengerCreateInfoEXT{
      .messageSeverity = severity_flags,
      .messageType     = msg_type_flags,
      .pfnUserCallback = &debug_callback,
  };

  if (auto debug_messenger_opt = instance_.createDebugUtilsMessengerEXT(debug_utils_messenger_create_info_ext)) {
    debug_messenger_ = std::move(debug_messenger_opt.value());
  } else {
    eray::util::Logger::err("Failed to create a vulkan debug messenger. Error type: {}",
                            vk::to_string(debug_messenger_opt.error()));
    return std::unexpected(Error{
        .msg     = "Debug Messenger Creation error",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = debug_messenger_opt.error(),
    });
  }

  return {};
}

Result<void, Error> Device::pick_physical_device(const CreateInfo& info) noexcept {
  auto devices_opt = instance_.enumeratePhysicalDevices();
  if (!devices_opt) {
    eray::util::Logger::err("Failed to enumerate physical devices. {}", vk::to_string(devices_opt.error()));
    return std::unexpected(Error{
        .msg     = "Physical devices enumeration failure",
        .code    = ErrorCode::PhysicalDeviceNotSufficient{},
        .vk_code = devices_opt.error(),
    });
  }
  auto devices = devices_opt.value();

  if (devices.empty()) {
    eray::util::Logger::err("Failed to find GPUs with Vulkan support.");
    return std::unexpected(Error{
        .msg  = "No physical device with Vulkan support",
        .code = ErrorCode::PhysicalDeviceNotSufficient{},
    });
  }

  // Ordered map for automatic sorting by device score
  auto candidates = std::multimap<uint32_t, vk::raii::PhysicalDevice>();
  for (const auto& device : devices) {
    auto props          = device.getProperties();  // name, type, supported Vulkan version
    auto queue_families = device.getQueueFamilyProperties();
    auto extensions     = device.enumerateDeviceExtensionProperties();

    auto supported = vk::False;
    vpGetPhysicalDeviceProfileSupport(*instance_, *device, &info.profile_properties, &supported);
    if (!supported) {
      util::Logger::info("Physical device with name {} is not suitable. This device will not be considered.",
                         std::string_view(props.deviceName));
      continue;
    }

    if (std::ranges::find_if(queue_families, [](const vk::QueueFamilyProperties& qfp) {
          return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
        }) == queue_families.end()) {
      continue;
    }

    auto found = true;
    for (const auto& extension : info.device_extensions) {
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
    return std::unexpected(Error{
        .msg  = "No physical device meeting the requirements",
        .code = ErrorCode::PhysicalDeviceNotSufficient{},
    });
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

vk::SampleCountFlagBits Device::max_usable_sample_count() const {
  auto props  = physical_device_.getProperties();
  auto counts = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;

  if (counts & vk::SampleCountFlagBits::e64) {
    return vk::SampleCountFlagBits::e64;
  }
  if (counts & vk::SampleCountFlagBits::e32) {
    return vk::SampleCountFlagBits::e32;
  }
  if (counts & vk::SampleCountFlagBits::e16) {
    return vk::SampleCountFlagBits::e16;
  }
  if (counts & vk::SampleCountFlagBits::e8) {
    return vk::SampleCountFlagBits::e8;
  }
  if (counts & vk::SampleCountFlagBits::e4) {
    return vk::SampleCountFlagBits::e4;
  }
  if (counts & vk::SampleCountFlagBits::e2) {
    return vk::SampleCountFlagBits::e2;
  }

  return vk::SampleCountFlagBits::e1;
}

Result<void, Error> Device::create_logical_device(const CreateInfo& info) noexcept {
  // ==  Find Required Queue Families ==================================================================================

  {
    auto queue_family_props         = physical_device_.getQueueFamilyProperties();
    auto indexed_queue_family_props = std::views::enumerate(queue_family_props);

    // Try to find a queue family that supports both presentation and graphics families.
    // Note: Vulkan requires an implementation which supports graphics operations to have at least one queue family that
    // supports both graphics and compute operations.
    auto queue_family_prop_it =
        std::ranges::find_if(indexed_queue_family_props, [&pd = physical_device_, &surf = surface_](auto&& pair) {
          auto&& [index, prop] = pair;
          return ((prop.queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)) !=
                  static_cast<vk::QueueFlags>(0)) &&
                 pd.getSurfaceSupportKHR(static_cast<uint32_t>(index), surf);
        });

    if (queue_family_prop_it == indexed_queue_family_props.end()) {
      // There is no a queue that supports both graphics and presentation queue families. We need separate queue
      // family.
      auto graphics_queue_family_prop_it = std::ranges::find_if(queue_family_props, [](const auto& prop) {
        return (prop.queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)) !=
               static_cast<vk::QueueFlags>(0);
      });

      if (graphics_queue_family_prop_it == queue_family_props.end()) {
        eray::util::Logger::err("Could not find a graphics queue family on the physical device");
        return std::unexpected(Error{
            .msg  = "Graphics queue family not supported on the physical device",
            .code = ErrorCode::PhysicalDeviceNotSufficient{},
        });
      }

      compute_queue_family_ = graphics_queue_family_ =
          static_cast<uint32_t>(std::distance(queue_family_props.begin(), graphics_queue_family_prop_it));

      auto surface_queue_family_prop_it =
          std::ranges::find_if(indexed_queue_family_props, [&pd = physical_device_, &surf = surface_](auto&& pair) {
            auto&& [index, prop] = pair;
            return pd.getSurfaceSupportKHR(static_cast<uint32_t>(index), surf);
          });

      if (surface_queue_family_prop_it == indexed_queue_family_props.end()) {
        eray::util::Logger::err("Could not find a presentation queue family on the physical device");
        return std::unexpected(Error{
            .msg  = "Presentation queue family not supported on the physical device",
            .code = ErrorCode::PhysicalDeviceNotSufficient{},
        });
      }

      presentation_queue_family_ =
          static_cast<uint32_t>(std::distance(indexed_queue_family_props.begin(), surface_queue_family_prop_it));
    } else {
      compute_queue_family_ = graphics_queue_family_ = presentation_queue_family_ =
          static_cast<uint32_t>(std::distance(indexed_queue_family_props.begin(), queue_family_prop_it));
    }
  }

  float queue_priority          = 0.F;
  auto device_queue_create_info = vk::DeviceQueueCreateInfo{
      .queueFamilyIndex = graphics_queue_family_,
      .queueCount       = 1,
      .pQueuePriorities = &queue_priority,  //
  };

  // == Logical Device Creation ========================================================================================

  auto device_create_info = vk::DeviceCreateInfo{
      .queueCreateInfoCount    = 1,
      .pQueueCreateInfos       = &device_queue_create_info,
      .enabledExtensionCount   = static_cast<uint32_t>(info.device_extensions.size()),
      .ppEnabledExtensionNames = info.device_extensions.data(),
  };

  auto vp_device_create_info                    = VpDeviceCreateInfo{};
  vp_device_create_info.pCreateInfo             = reinterpret_cast<const VkDeviceCreateInfo*>(&device_create_info);
  vp_device_create_info.enabledFullProfileCount = 1;
  vp_device_create_info.pEnabledFullProfiles    = &info.profile_properties;

  VkDevice vp_device = nullptr;
  if (auto result = vk::Result(vpCreateDevice(*physical_device_, &vp_device_create_info, nullptr, &vp_device));
      result != vk::Result::eSuccess) {
    eray::util::Logger::err("Failed to create a logical device. {}", vk::to_string(result));
    return std::unexpected(Error{
        .msg     = "Vulkan Logical Device creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = result,
    });
  }
  auto device = vk::Device{vp_device};
  device_     = vk::raii::Device{physical_device_, device};

  // ==  Queues Creation ===============================================================================================

  if (auto result = device_.getQueue(graphics_queue_family_, 0)) {
    graphics_queue_ = std::move(*result);
  } else {
    eray::util::Logger::err("Failed to create a graphics queue. {}", vk::to_string(result.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan Graphics Queue creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = result.error(),
    });
  }

  if (auto result = device_.getQueue(compute_queue_family_, 0)) {
    compute_queue_ = std::move(*result);
  } else {
    eray::util::Logger::err("Failed to create a compute queue. {}", vk::to_string(result.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan Compute Queue creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = result.error(),
    });
  }

  if (auto result = device_.getQueue(presentation_queue_family_, 0)) {
    presentation_queue_ = std::move(*result);
  } else {
    eray::util::Logger::err("Failed to create a presentation queue. {}", vk::to_string(result.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan Presentation Queue creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = result.error(),
    });
  }

  return {};
}

Result<void, Error> Device::create_command_pool() noexcept {
  auto command_pool_info = vk::CommandPoolCreateInfo{
      // There are two possible flags for command pools:
      // - VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often
      //   (may change memory allocation behavior).
      // - VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually,
      //   without this flag they all have to be reset together. (Reset and rerecord over it in every frame)

      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,

      // Each command pool can only allocate command buffers that are submitted on a single type of queue.
      // We setup commands for drawing, and thus we've chosen the graphics queue family.
      .queueFamilyIndex = graphics_queue_family(),
  };

  if (auto result = device_.createCommandPool(command_pool_info)) {
    single_time_cmd_pool_ = std::move(*result);
  } else {
    eray::util::Logger::err("Could not create a command pool. {}", vk::to_string(result.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan Command Pool creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = result.error(),
    });
  }

  return {};
}

std::vector<const char*> Device::global_extensions(const CreateInfo& info) noexcept {
  auto result = std::vector<const char*>(info.global_extensions.begin(), info.global_extensions.end());

  if (!info.validation_layers.empty()) {
    result.push_back(vk::EXTDebugUtilsExtensionName);
  }
  return result;
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Device::debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
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

Result<uint32_t, Error> Device::find_mem_type(uint32_t type_filter, vk::MemoryPropertyFlags props) const {
  // Graphics cards can offer different types of memory to allocate from. Each type of memory varies in terms of
  // allowed operations and performance characteristics. We need to combine the requirements of the buffer and
  // our own app requirements to find the right type of memory to use.

  // memoryHeaps: distinct memory resources like dedicated VRAM and swap space in RAM for when VRAM runs out.
  // Different types of memory exist within these heaps.
  auto mem_props = physical_device().getMemoryProperties();

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
    if ((((type_filter) & (1 << i)) != 0U) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }

  util::Logger::err("Could not find a memory type");

  return std::unexpected(Error{
      .msg  = "No suitable memory type",
      .code = ErrorCode::NoSuitableMemoryTypeFailure{},
  });
}

vk::raii::CommandBuffer Device::begin_single_time_commands(OptRef<vk::raii::CommandPool> command_pool) const {
  auto cmd_buff_info = vk::CommandBufferAllocateInfo{
      .commandPool        = command_pool ? *command_pool : single_time_cmd_pool_,
      .level              = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  auto cmd_buff = std::move(device_.allocateCommandBuffers(cmd_buff_info)->front());
  cmd_buff.begin(vk::CommandBufferBeginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  });

  return cmd_buff;
}

void Device::end_single_time_commands(vk::raii::CommandBuffer& cmd_buff) const {
  cmd_buff.end();

  const auto submit_info = vk::SubmitInfo{
      .waitSemaphoreCount   = 0,
      .pWaitSemaphores      = nullptr,
      .pWaitDstStageMask    = nullptr,
      .commandBufferCount   = 1,
      .pCommandBuffers      = &*cmd_buff,
      .signalSemaphoreCount = 0,
      .pSignalSemaphores    = nullptr,  //
  };
  graphics_queue().submit(submit_info, nullptr);
  graphics_queue().waitIdle();  // equivalent of having submitted a valid fence to every previously executed queue
                                // submission command
}

void Device::transition_image_layout(const vk::raii::Image& image, const ImageDescription& image_desc, bool mipmapping,
                                     vk::ImageLayout old_layout, vk::ImageLayout new_layout) const {
  auto cmd_buff = begin_single_time_commands();
  auto barrier  = vk::ImageMemoryBarrier{
       .oldLayout           = old_layout,
       .newLayout           = new_layout,
       .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
       .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
       .image               = image,  //
       .subresourceRange =
          vk::ImageSubresourceRange{
               .aspectMask     = vk::ImageAspectFlagBits::eColor,
               .baseMipLevel   = 0,
               .levelCount     = mipmapping ? image_desc.find_mip_levels() : 1,
               .baseArrayLayer = 0,
               .layerCount     = 1,
          },
  };

  auto src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
  auto dst_stage = vk::PipelineStageFlagBits::eTransfer;
  if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
    dst_stage = vk::PipelineStageFlagBits::eTransfer;

  } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
             new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    src_stage = vk::PipelineStageFlagBits::eTransfer;
    dst_stage = vk::PipelineStageFlagBits::eFragmentShader;

  } else {
    eray::util::panic("Unsupported layout transition");
  }

  // Defines memory dependency between commands that were submitted to the same queue
  cmd_buff.pipelineBarrier(src_stage, dst_stage, {}, {}, nullptr, barrier);

  end_single_time_commands(cmd_buff);
}

void Device::cleanup() { main_deletion_queue_.flush(); }

Device::~Device() { cleanup(); }

}  // namespace eray::vkren
