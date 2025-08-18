#include <algorithm>
#include <liberay/util/logger.hpp>
#include <liberay/util/try.hpp>
#include <liberay/vkren/device.hpp>
#include <map>
#include <ranges>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

Device::CreateInfo Device::CreateInfo::DesktopTemplate::get(
    const SurfaceCreator& surface_creator_func, std::span<const char* const> required_global_extensions) noexcept {
  // == Validation Layers ============================================================================================
  validation_layers_.clear();
#ifndef NDEBUG
  validation_layers_.emplace_back("VK_LAYER_KHRONOS_validation");
#endif

  // == Global Extensions ============================================================================================
  global_extensions_.clear();
  global_extensions_.resize(required_global_extensions.size());
  std::ranges::copy(required_global_extensions.begin(), required_global_extensions.end(), global_extensions_.begin());
  // vk::EXTDebugUtilsExtensionName is added automatically if validation layers are not empty

  // == Device Extensions ============================================================================================
  device_extensions_.clear();
  device_extensions_ = {
      vk::KHRSwapchainExtensionName,             // requires Surface Instance Extension
      vk::KHRSpirv14ExtensionName,               //
      vk::KHRShaderDrawParametersExtensionName,  // BaseInstance, BaseVertex, DrawIndex
      vk::KHRSynchronization2ExtensionName,      //
      vk::KHRCreateRenderpass2ExtensionName      //
  };

  // == Feature Chain ================================================================================================
  feature_chain_ = vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                                      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>{
      {
          .features =
              vk::PhysicalDeviceFeatures{
                  .samplerAnisotropy = vk::True,

              },
      },                                                             // vk::PhysicalDeviceFeatures2
      {.synchronization2 = vk::True, .dynamicRendering = vk::True},  // Enable dynamic rendering from Vulkan 1.3
      {.extendedDynamicState = vk::True}                             // Enable extended dynamic state from the extension
  };

  // == App info =====================================================================================================
  auto app_create_info = vk::ApplicationInfo{
      .pApplicationName   = "Vulkan Application",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName        = "No Engine",
      .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion         = vk::ApiVersion14  //
  };

  return CreateInfo{
      .validation_layers = validation_layers_,
      .global_extensions = global_extensions_,
      .device_extensions = device_extensions_,
      .surface_creator   = surface_creator_func,
      .feature_chain     = feature_chain_.get<vk::PhysicalDeviceFeatures2, 0>(),
      .severity_flags =
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
      .app_info = std::move(app_create_info),
  };
}

Result<Device, Device::CreationError> Device::create(vk::raii::Context& ctx, const CreateInfo& info) noexcept {
  auto device = Device();
  TRY(device.create_instance(ctx, info));
  TRY(device.create_debug_messenger(info));
  if (auto result = info.surface_creator(device.instance())) {
    device.surface_ = std::move(*result);
  } else {
    util::Logger::err("Failed to create a surface with the injected surface creator.");
    return std::unexpected(result.error());
  }
  TRY(device.pick_physical_device(info));
  TRY(device.create_logical_device(info));
  TRY(device.create_command_pool());

  return device;
}

Result<void, Device::InstanceCreationError> Device::create_instance(vk::raii::Context& ctx,
                                                                    const CreateInfo& info) noexcept {
  auto extensions_props = ctx.enumerateInstanceExtensionProperties();

  // Check if the requested extensions are supported by the Vulkan implementation.
  auto global_extensions = get_global_extensions(info);
  for (const auto& ext : global_extensions) {
    if (std::ranges::none_of(extensions_props, [ext_name = std::string_view(ext)](const auto& prop) {
          return std::string_view(prop.extensionName) == ext_name;
        })) {
      eray::util::Logger::err("Failed to create a Vulkan Instance. Requested extension {} is not supported", ext);
      return std::unexpected(InstanceCreationError{});
    }
  }

  // Check if the requested validation layers are supported by the Vulkan implementation.
  auto layer_props = ctx.enumerateInstanceLayerProperties();
  if (std::ranges::any_of(info.validation_layers, [&layer_props](const auto& required_layer) {
        return std::ranges::none_of(layer_props, [required_layer](auto const& layer_prop) {
          return required_layer == std::string_view(layer_prop.layerName);
        });
      })) {
    eray::util::Logger::err("Failed to create a Vulkan Instance. Use of unsupported validation layer(s).");
    return std::unexpected(InstanceCreationError{});
  }

  auto instance_info = vk::InstanceCreateInfo{
      // app info
      .pApplicationInfo = &info.app_info,
      // validation layers
      .enabledLayerCount   = static_cast<uint32_t>(info.validation_layers.size()),
      .ppEnabledLayerNames = info.validation_layers.data(),
      // global extensions
      .enabledExtensionCount   = static_cast<uint32_t>(global_extensions.size()),
      .ppEnabledExtensionNames = global_extensions.data(),  //
  };

  if (auto result = ctx.createInstance(instance_info)) {
    instance_ = std::move(*result);
  } else {
    eray::util::Logger::err("Failed to create a Vulkan Instance. Error type: {}", vk::to_string(result.error()));
    return std::unexpected(InstanceCreationError{});
  }

  eray::util::Logger::succ("Successfully created a Vulkan Instance");

  return {};
}

Result<void, Device::DebugMessengerCreationError> Device::create_debug_messenger(const CreateInfo& info) noexcept {
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
    return std::unexpected(DebugMessengerCreationError{});
  }

  return {};
}

Result<void, Device::PhysicalDevicePickingError> Device::pick_physical_device(const CreateInfo& info) noexcept {
  auto devices_opt = instance_.enumeratePhysicalDevices();
  if (!devices_opt) {
    eray::util::Logger::err("Failed to enumerate physical devices. {}", vk::to_string(devices_opt.error()));
    return std::unexpected(PhysicalDevicePickingError{});
  }
  auto devices = devices_opt.value();

  if (devices.empty()) {
    eray::util::Logger::err("Failed to find GPUs with Vulkan support.");
    return std::unexpected(PhysicalDevicePickingError{});
  }

  // Ordered map for automatic sorting by device score
  auto candidates = std::multimap<uint32_t, vk::raii::PhysicalDevice>();

  for (const auto& device : devices) {
    auto props    = device.getProperties();  // name, type, supported Vulkan version
    auto features = device.getFeatures();    // optional features like texture compression, 64-bit floats, multi-view
                                             // port rendering (VR)
    auto queue_families = device.getQueueFamilyProperties();
    auto extensions     = device.enumerateDeviceExtensionProperties();

    if (!features.geometryShader || !features.tessellationShader || !features.samplerAnisotropy) {
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
    return std::unexpected(PhysicalDevicePickingError{});
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

Result<void, Device::LogicalDeviceCreationError> Device::create_logical_device(const CreateInfo& info) noexcept {
  // ==  Find Required Queue Families ==================================================================================

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
        std::unexpected(LogicalDeviceCreationError{});
      }

      graphics_queue_family_ =
          static_cast<uint32_t>(std::distance(queue_family_props.begin(), graphics_queue_family_prop_it));

      auto surface_queue_family_prop_it =
          std::ranges::find_if(indexed_queue_family_props, [&pd = physical_device_, &surf = surface_](auto&& pair) {
            auto&& [index, prop] = pair;
            return pd.getSurfaceSupportKHR(static_cast<uint32_t>(index), surf);
          });

      if (surface_queue_family_prop_it == indexed_queue_family_props.end()) {
        eray::util::Logger::err("Could not find a presentation queue family on the physical device");
        std::unexpected(LogicalDeviceCreationError{});
      }

      presentation_queue_family_ =
          static_cast<uint32_t>(std::distance(indexed_queue_family_props.begin(), surface_queue_family_prop_it));
    } else {
      graphics_queue_family_ = presentation_queue_family_ =
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

  auto default_feature_chain = get_default_feature_chain();
  auto device_create_info    = vk::DeviceCreateInfo{
         .pNext =
          info.feature_chain ? info.feature_chain : default_feature_chain.get<vk::PhysicalDeviceFeatures2, 0>(),  //
         .queueCreateInfoCount    = 1,                                                                               //
         .pQueueCreateInfos       = &device_queue_create_info,                                                       //
         .enabledExtensionCount   = static_cast<uint32_t>(info.device_extensions.size()),
         .ppEnabledExtensionNames = info.device_extensions.data(),
  };

  if (auto result = physical_device_.createDevice(device_create_info)) {
    device_ = std::move(*result);
  } else {
    eray::util::Logger::err("Failed to create a logical device. {}", vk::to_string(result.error()));
    return std::unexpected(LogicalDeviceCreationError{});
  }

  // ==  Queues Creation ===============================================================================================

  if (auto result = device_.getQueue(graphics_queue_family_, 0)) {
    graphics_queue_ = std::move(*result);
  } else {
    eray::util::Logger::err("Failed to create a graphics queue. {}", vk::to_string(result.error()));
    return std::unexpected(LogicalDeviceCreationError{});
  }

  if (auto result = device_.getQueue(presentation_queue_family_, 0)) {
    presentation_queue_ = std::move(*result);
  } else {
    eray::util::Logger::err("Failed to create a presentation queue. {}", vk::to_string(result.error()));
    return std::unexpected(LogicalDeviceCreationError{});
  }

  return {};
}

Result<void, Device::CommandPoolCreationError> Device::create_command_pool() noexcept {
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

    return std::unexpected(CommandPoolCreationError{});
  }

  return {};
}

std::vector<const char*> Device::get_global_extensions(const CreateInfo& info) noexcept {
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

Result<uint32_t, Device::NoSuitableMemoryTypeError> Device::find_mem_type(uint32_t type_filter,
                                                                          vk::MemoryPropertyFlags props) const {
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

  return std::unexpected(NoSuitableMemoryTypeError{});
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

void Device::transition_image_layout(const vk::raii::Image& image, vk::ImageLayout old_layout,
                                     vk::ImageLayout new_layout) const {
  auto cmd_buff = begin_single_time_commands();
  auto barrier  = vk::ImageMemoryBarrier{
       .oldLayout           = old_layout,
       .newLayout           = new_layout,
       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
       .image               = image,  //
       .subresourceRange =
          vk::ImageSubresourceRange{
               .aspectMask     = vk::ImageAspectFlagBits::eColor,
               .baseMipLevel   = 0,
               .levelCount     = 1,
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

}  // namespace eray::vkren
