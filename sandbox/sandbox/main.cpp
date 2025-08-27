#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/system.hpp>
#include <liberay/res/image.hpp>
#include <liberay/res/shader.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/memory_region.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/shader.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <sandbox/particle.hpp>
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

namespace vkren = eray::vkren;

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
struct NoSuitableMemoryType {};
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
                 VulkanSwapChainSupportIsNotSufficient, FileError, VulkanObjectCreationError, NoSuitableMemoryType>;
using AppError = std::variant<GLFWWindowCreationFailure, VulkanInitError>;

struct SwapchainRecreationFailure {};
struct SwapChainImageAcquireFailure {};

using DrawFrameError = std::variant<SwapchainRecreationFailure, SwapChainImageAcquireFailure>;

class HelloTriangleApplication {
 public:
  std::expected<void, GLFWWindowCreationFailure> run() {
    TRY(initWindow());
    init_vk();
    main_loop();
    cleanup();

    return {};
  }

  static constexpr uint32_t kWinWidth  = 800;
  static constexpr uint32_t kWinHeight = 600;

 private:
  void init_vk() {
    create_device();
    create_swap_chain();
    create_descriptor_set_layout();
    create_graphics_pipeline();
    create_compute_pipeline();
    create_command_pool();
    create_buffers();
    create_command_buffers();
    create_descriptor_pool();
    create_descriptor_sets();
    create_sync_objs();
  }

  void create_device() {
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
    auto surface_creator = [this](const vk::raii::Instance& instance) -> std::optional<vk::raii::SurfaceKHR> {
      VkSurfaceKHR surface{};
      if (glfwCreateWindowSurface(*instance, window_, nullptr, &surface)) {
        eray::util::Logger::info("Could not create a window surface");
        return std::nullopt;
      }

      return vk::raii::SurfaceKHR(instance, surface);
    };

    // == Device Creation ==============================================================================================
    auto desktop_template                 = vkren::Device::CreateInfo::DesktopProfile{};
    auto device_info                      = desktop_template.get(surface_creator, required_global_extensions);
    device_info.app_info.pApplicationName = "VkTriangle";
    device_ = vkren::Device::create(context_, device_info).or_panic("Could not create a logical device wrapper");
  }

  std::expected<void, GLFWWindowCreationFailure> initWindow() {
    glfwInit();

    if (!glfwVulkanSupported()) {
      eray::util::panic("GLFW could not load Vulkan");
    }

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

  void main_loop() {
    static auto prev_time = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
      if (!draw_frame()) {
        eray::util::Logger::err("Closing window: Failed to draw a frame");
        break;
      }
      auto curr_time   = std::chrono::high_resolution_clock::now();
      last_frame_time_ = std::chrono::duration<float, std::chrono::seconds::period>(curr_time - prev_time).count();
      prev_time        = curr_time;
    }

    // Since draw frame operations are async, when the main loop ends the drawing operations may still be going on.
    // This call is allows for the async operations to finish before cleaning the resources.
    device_->waitIdle();
  }

  std::expected<void, DrawFrameError> draw_frame() {
    auto [result, image_index] = swap_chain_->acquireNextImage(UINT64_MAX, nullptr, *in_flight_fences_[current_frame_]);
    while (vk::Result::eTimeout == device_->waitForFences(*in_flight_fences_[current_frame_], vk::True, UINT64_MAX)) {
    }
    device_->resetFences(*in_flight_fences_[current_frame_]);

    uint64_t compute_wait_value    = timeline_value_;
    uint64_t compute_signal_value  = ++timeline_value_;
    uint64_t graphics_wait_value   = compute_signal_value;
    uint64_t graphics_signal_value = ++timeline_value_;

    // == Compute Submission ===========================================================================================
    {
      update_ubo(current_frame_);
      record_compute_command_buffer(current_frame_);

      const auto timeline_info = vk::TimelineSemaphoreSubmitInfo{
          .waitSemaphoreValueCount   = 1,
          .pWaitSemaphoreValues      = &compute_wait_value,
          .signalSemaphoreValueCount = 1,
          .pSignalSemaphoreValues    = &compute_signal_value,
      };
      vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eComputeShader};
      const auto submit_info               = vk::SubmitInfo{
                        .pNext                = &timeline_info,
                        .waitSemaphoreCount   = 1,
                        .pWaitSemaphores      = &*timeline_semaphore_,
                        .pWaitDstStageMask    = wait_stages,
                        .commandBufferCount   = 1,
                        .pCommandBuffers      = &*compute_command_buffers_[current_frame_],
                        .signalSemaphoreCount = 1,
                        .pSignalSemaphores    = &*timeline_semaphore_,
      };
      device_.compute_queue().submit(submit_info, nullptr);
    }

    // == Graphics Submission ==========================================================================================
    {
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

      record_graphics_command_buffer(current_frame_, image_index);

      vk::PipelineStageFlags wait_destination_stage_mask[] = {vk::PipelineStageFlagBits::eVertexInput,
                                                              vk::PipelineStageFlagBits::eColorAttachmentOutput};

      const auto timeline_info = vk::TimelineSemaphoreSubmitInfo{
          .waitSemaphoreValueCount   = 1,
          .pWaitSemaphoreValues      = &graphics_wait_value,
          .signalSemaphoreValueCount = 1,
          .pSignalSemaphoreValues    = &graphics_signal_value,
      };
      const auto submit_info = vk::SubmitInfo{
          .pNext                = &timeline_info,
          .waitSemaphoreCount   = 1,
          .pWaitSemaphores      = &*timeline_semaphore_,
          .pWaitDstStageMask    = wait_destination_stage_mask,
          .commandBufferCount   = 1,
          .pCommandBuffers      = &*graphics_command_buffers_[current_frame_],
          .signalSemaphoreCount = 1,
          .pSignalSemaphores    = &*timeline_semaphore_,  //
      };
      device_.graphics_queue().submit(submit_info, nullptr);

      auto wait_info = vk::SemaphoreWaitInfo{
          .semaphoreCount = 1,
          .pSemaphores    = &*timeline_semaphore_,
          .pValues        = &graphics_signal_value,
      };

      // Block the CPU until the graphics and compute are ready for presentation
      while (vk::Result::eTimeout == device_->waitSemaphores(wait_info, UINT64_MAX)) {
      }

      const auto present_info = vk::PresentInfoKHR{
          .waitSemaphoreCount = 0,
          .pWaitSemaphores    = nullptr,
          .swapchainCount     = 1,
          .pSwapchains        = &**swap_chain_,
          .pImageIndices      = &image_index,
      };
      result = device_.presentation_queue().presentKHR(present_info);

      if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebuffer_resized_) {
        framebuffer_resized_ = false;
        TRY(recreate_swap_chain());
      } else if (result != vk::Result::eSuccess) {
        eray::util::Logger::err("Failed to present swap chain image");
        return std::unexpected(SwapChainImageAcquireFailure{});
      }
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;

    return {};
  }

  void update_ubo(uint32_t frame_index) {
    auto ubo = UniformBufferObject{
        .delta_time = last_frame_time_,
    };
    memcpy(uniform_buffers_mapped_[frame_index], &ubo, sizeof(ubo));
  }

  void cleanup() {
    swap_chain_.cleanup();

    glfwDestroyWindow(window_);
    glfwTerminate();

    eray::util::Logger::succ("Finished cleanup");
  }

  void create_swap_chain() {
    // Unfortunately, if you are using a high DPI display (like Apple’s Retina display), screen coordinates don’t
    // correspond to pixels. For that reason we use glfwGetFrameBufferSize to get size in pixels. (Note:
    // glfwGetWindowSize returns size in screen coordinates).
    int width{};
    int height{};
    glfwGetFramebufferSize(window_, &width, &height);
    swap_chain_ = vkren::SwapChain::create(device_, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                           device_.get_max_usable_sample_count())
                      .or_panic("Could not create a swap chain");
  }

  std::expected<void, SwapchainRecreationFailure> recreate_swap_chain() {
    int width  = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window_, &width, &height);
      glfwWaitEvents();
    }

    if (!swap_chain_.recreate(device_, static_cast<uint32_t>(width), static_cast<uint32_t>(height))) {
      return std::unexpected(SwapchainRecreationFailure{});
    }

    return {};
  }

  void create_graphics_pipeline() {
    // == 1. Shader stage ==============================================================================================
    auto main_binary =
        eray::res::SPIRVShaderBinary::load_from_path(eray::os::System::executable_dir() / "shaders" / "main.spv")
            .or_panic("Could not find main graphics shader");
    auto main_shader_module =
        vkren::ShaderModule::create(device_, main_binary).or_panic("Could not create a main shader module");

    auto vert_shader_stage_pipeline_info = vk::PipelineShaderStageCreateInfo{
        .stage  = vk::ShaderStageFlagBits::eVertex,
        .module = main_shader_module.shader_module,
        .pName  = kVertexShaderEntryPoint.c_str(),
    };

    auto frag_shader_stage_pipeline_info = vk::PipelineShaderStageCreateInfo{
        .stage  = vk::ShaderStageFlagBits::eFragment,
        .module = main_shader_module.shader_module,
        .pName  = kFragmentShaderEntryPoint.c_str(),
    };

    auto shader_stages = std::array<vk::PipelineShaderStageCreateInfo, 2>{vert_shader_stage_pipeline_info,
                                                                          frag_shader_stage_pipeline_info};

    // == 2. Dynamic state =============================================================================================
    auto dynamic_states = std::vector{
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    auto dynamic_state = vk::PipelineDynamicStateCreateInfo{
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates    = dynamic_states.data(),
    };
    auto viewport_state_info = vk::PipelineViewportStateCreateInfo{.viewportCount = 1, .scissorCount = 1};

    // == 3. Input assembly ============================================================================================
    auto binding_desc       = ParticleSystem::get_binding_desc();
    auto attribs_desc       = ParticleSystem::get_attribs_desc();
    auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo{
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding_desc,
        .vertexAttributeDescriptionCount = attribs_desc.size(),
        .pVertexAttributeDescriptions    = attribs_desc.data(),
    };
    auto input_assembly = vk::PipelineInputAssemblyStateCreateInfo{
        .topology               = vk::PrimitiveTopology::ePointList,
        .primitiveRestartEnable = vk::False,
    };

    // == 4. Rasterizer ================================================================================================
    auto rasterization_state_info = vk::PipelineRasterizationStateCreateInfo{
        .depthClampEnable     = vk::False,
        .polygonMode          = vk::PolygonMode::eFill,
        .cullMode             = vk::CullModeFlagBits::eBack,
        .frontFace            = vk::FrontFace::eClockwise,
        .depthBiasEnable      = vk::False,
        .depthBiasSlopeFactor = 1.0F,

        // NOTE: The maximum line width that is supported depends on the hardware and any lin thicker
        // than 1.0F requires to enable the wideLines GPU feature.
        .lineWidth = 1.0F,
    };

    // == 5. Multisampling =============================================================================================
    auto multisampling_state_info = vk::PipelineMultisampleStateCreateInfo{
        .rasterizationSamples = swap_chain_.msaa_sample_count(),
    };

    // == 6. Depth and Stencil Testing =================================================================================
    auto depth_stencil_state_info = vk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable       = vk::False,
        .depthWriteEnable      = vk::False,
        .depthCompareOp        = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable     = vk::False,
    };

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
    auto pipeline_layout_info = vk::PipelineLayoutCreateInfo{
        .setLayoutCount         = 0,
        .pushConstantRangeCount = 0,
    };

    graphics_pipeline_layout_ = vkren::Result(device_->createPipelineLayout(pipeline_layout_info))
                                    .or_panic("Could not create a pipeline layout");

    // == 9. Graphics Pipeline  ========================================================================================

    // We use the dynamic rendering feature (Vulkan 1.3), the structure below specifies color attachment data, and
    // the format. In previous versions of Vulkan, we would need to create framebuffers to bind our image views to
    // a render pass, so the dynamic rendering eliminates the need for render pass and framebuffer.
    auto format = swap_chain_.color_attachment_format();
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info{
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &format,
        .depthAttachmentFormat   = swap_chain_.depth_stencil_attachment_format(),
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
        .pDepthStencilState  = &depth_stencil_state_info,
        .pColorBlendState    = &color_blending_info,
        .pDynamicState       = &dynamic_state,
        .layout              = graphics_pipeline_layout_,

        .renderPass = nullptr,  // we are using dynamic rendering

        // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex  = -1,
    };

    graphics_pipeline_ = vkren::Result(device_->createGraphicsPipeline(nullptr, pipeline_info))
                             .or_panic("Could not create a graphics pipeline.");
  }

  void create_compute_pipeline() {
    // == 1. Shader stage ==============================================================================================
    auto particle_binary =
        eray::res::SPIRVShaderBinary::load_from_path(eray::os::System::executable_dir() / "shaders" / "particle.spv")
            .or_panic("Could not find particle compute shader");
    auto particle_shader_module =
        vkren::ShaderModule::create(device_, particle_binary).or_panic("Could not create a main shader module");

    auto compute_shader_stage = vk::PipelineShaderStageCreateInfo{
        .stage  = vk::ShaderStageFlagBits::eCompute,     //
        .module = particle_shader_module.shader_module,  //
        .pName  = kComputeShaderEntryPoint.c_str(),      // entry point name
    };

    // == 2. Layout creation ===========================================================================================
    auto pipeline_layout_info = vk::PipelineLayoutCreateInfo{
        .setLayoutCount         = 1,
        .pSetLayouts            = &*compute_descriptor_set_layout_,
        .pushConstantRangeCount = 0,
    };

    compute_pipeline_layout_ = vkren::Result(device_->createPipelineLayout(pipeline_layout_info))
                                   .or_panic("Could not create a pipeline layout");

    // == 3. Compute Pipeline  =========================================================================================
    auto pipeline_info = vk::ComputePipelineCreateInfo{
        .stage  = compute_shader_stage,
        .layout = compute_pipeline_layout_,
    };
    compute_pipeline_ = vkren::Result(device_->createComputePipeline(nullptr, pipeline_info))
                            .or_panic("Could not create a graphics pipeline.");
  }

  void create_command_pool() {
    if (device_.graphics_queue_family() != device_.compute_queue_family()) {
      eray::util::panic("Expected graphics queue and compute queue to be the same");
    }

    auto command_pool_info = vk::CommandPoolCreateInfo{
        .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = device_.graphics_queue_family(),
    };

    command_pool_ =
        vkren::Result(device_->createCommandPool(command_pool_info)).or_panic("Could not create a command pool.");
  }

  void create_buffers() {
    // == Storage Buffers ==============================================================================================
    auto particle_system =
        ParticleSystem::create_on_circle(static_cast<float>(kWinWidth) / static_cast<float>(kWinHeight));
    auto region =
        eray::util::MemoryRegion{particle_system.particles.data(), particle_system.particles.size() * sizeof(Particle)};
    auto staging_buff = vkren::ExclusiveBufferResource::create_staging_buffer(device_, region)
                            .or_panic("Could not create a Staging Buffer");

    for (auto i = 0U; i < kMaxFramesInFlight; ++i) {
      auto temp = vkren::ExclusiveBufferResource::create(
                      device_,
                      vkren::ExclusiveBufferResource::CreateInfo{
                          .size_bytes = region.size_bytes(),
                          .buff_usage = vk::BufferUsageFlagBits::eVertexBuffer |
                                        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                          .mem_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                      })
                      .or_panic("Could not create a Storage Buffer");

      temp.copy_from(staging_buff.buffer(), vk::BufferCopy{
                                                .srcOffset = 0,
                                                .dstOffset = 0,
                                                .size      = region.size_bytes(),
                                            });
      ssbuffers_.emplace_back(std::move(temp));
    }

    // == Uniform Buffers ==============================================================================================
    uniform_buffers_.clear();
    uniform_buffers_mapped_.clear();
    {
      vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
      for (auto i = 0; i < kMaxFramesInFlight; ++i) {
        auto ubo = vkren::ExclusiveBufferResource::create(  //
                       device_,
                       vkren::ExclusiveBufferResource::CreateInfo{
                           .size_bytes = buffer_size,
                           .buff_usage = vk::BufferUsageFlagBits::eUniformBuffer,
                           .mem_properties =
                               vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                       })
                       .or_panic();

        // This technique is called persistent mapping, the buffer stays mapped for the application's whole life-time.
        // It increases performance as the mapping process is not free.
        uniform_buffers_mapped_.emplace_back(ubo.memory().mapMemory(0, buffer_size));
        uniform_buffers_.emplace_back(std::move(ubo));
      }
    }
  }

  void create_command_buffers() {
    {
      auto alloc_info = vk::CommandBufferAllocateInfo{
          .commandPool        = command_pool_,
          .level              = vk::CommandBufferLevel::ePrimary,  //
          .commandBufferCount = kMaxFramesInFlight,                //
      };

      graphics_command_buffers_ =
          vkren::Result(device_->allocateCommandBuffers(alloc_info)).or_panic("Command buffer allocation failure.");
    }

    {
      auto alloc_info = vk::CommandBufferAllocateInfo{
          .commandPool        = command_pool_,
          .level              = vk::CommandBufferLevel::ePrimary,  //
          .commandBufferCount = kMaxFramesInFlight,                //
      };

      compute_command_buffers_ =
          vkren::Result(device_->allocateCommandBuffers(alloc_info)).or_panic("Command buffer allocation failure.");
    }
  }

  void create_sync_objs() {
    auto semaphore_info = vk::SemaphoreTypeCreateInfo{
        .semaphoreType = vk::SemaphoreType::eTimeline,
        .initialValue  = 0,
    };
    timeline_semaphore_ = vkren::Result(device_->createSemaphore(vk::SemaphoreCreateInfo{.pNext = &semaphore_info}))
                              .or_panic("Could not create a semaphore");
    timeline_value_ = 0;
    in_flight_fences_.clear();
    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
      in_flight_fences_.emplace_back(
          vkren::Result(device_->createFence(vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled}))
              .or_panic("Could not create a fence"));

      device_->resetFences(*in_flight_fences_.back());
    }
  }

  struct TransitionSwapChainImageLayoutInfo {
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
  void transition_swap_chain_image_layout(TransitionSwapChainImageLayoutInfo info) {
    auto barrier = vk::ImageMemoryBarrier2{
        .srcStageMask        = info.src_stage_mask,
        .srcAccessMask       = info.src_access_mask,
        .dstStageMask        = info.dst_stage_mask,
        .dstAccessMask       = info.dst_access_mask,
        .oldLayout           = info.old_layout,
        .newLayout           = info.new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swap_chain_.images()[info.image_index],  //
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

    graphics_command_buffers_[info.frame_index].pipelineBarrier2(dependency_info);
  }

  struct TransitionDepthAttachmentLayoutInfo {
    size_t frame_index;
    vk::ImageLayout old_layout;
    vk::ImageLayout new_layout;
    vk::AccessFlags2 src_access_mask;
    vk::AccessFlags2 dst_access_mask;
    vk::PipelineStageFlags2 src_stage_mask;
    vk::PipelineStageFlags2 dst_stage_mask;
  };

  void transition_depth_attachment_layout(TransitionDepthAttachmentLayoutInfo info) {
    auto barrier = vk::ImageMemoryBarrier2{
        .srcStageMask        = info.src_stage_mask,
        .srcAccessMask       = info.src_access_mask,
        .dstStageMask        = info.dst_stage_mask,
        .dstAccessMask       = info.dst_access_mask,
        .oldLayout           = info.old_layout,
        .newLayout           = info.new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swap_chain_.depth_stencil_attachment_image(),  //
        .subresourceRange =
            vk::ImageSubresourceRange{
                .aspectMask     = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
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

    graphics_command_buffers_[info.frame_index].pipelineBarrier2(dependency_info);
  }

  struct TransitionColorAttachmentLayoutInfo {
    size_t frame_index;
    vk::ImageLayout old_layout;
    vk::ImageLayout new_layout;
    vk::AccessFlags2 src_access_mask;
    vk::AccessFlags2 dst_access_mask;
    vk::PipelineStageFlags2 src_stage_mask;
    vk::PipelineStageFlags2 dst_stage_mask;
  };

  void transition_color_attachment_layout(TransitionColorAttachmentLayoutInfo info) {
    auto barrier = vk::ImageMemoryBarrier2{
        .srcStageMask        = info.src_stage_mask,
        .srcAccessMask       = info.src_access_mask,
        .dstStageMask        = info.dst_stage_mask,
        .dstAccessMask       = info.dst_access_mask,
        .oldLayout           = info.old_layout,
        .newLayout           = info.new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swap_chain_.color_attachment_image(),  //
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

    graphics_command_buffers_[info.frame_index].pipelineBarrier2(dependency_info);
  }

  /**
   * @brief Writes the commands we what to execute into a command buffer
   *
   * @param image_index
   */
  void record_graphics_command_buffer(size_t frame_index, uint32_t image_index) {
    graphics_command_buffers_[frame_index].begin(vk::CommandBufferBeginInfo{});

    // Transition the image layout from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
    transition_swap_chain_image_layout(TransitionSwapChainImageLayoutInfo{
        .image_index     = image_index,
        .frame_index     = frame_index,
        .old_layout      = vk::ImageLayout::eUndefined,
        .new_layout      = vk::ImageLayout::eColorAttachmentOptimal,
        .src_access_mask = {},
        .dst_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .src_stage_mask  = vk::PipelineStageFlagBits2::eTopOfPipe,
        .dst_stage_mask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    });

    transition_depth_attachment_layout(TransitionDepthAttachmentLayoutInfo{
        .frame_index     = frame_index,
        .old_layout      = vk::ImageLayout::eUndefined,
        .new_layout      = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .src_access_mask = {},
        .dst_access_mask =
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
        .src_stage_mask = vk::PipelineStageFlagBits2::eTopOfPipe,
        .dst_stage_mask =
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
    });

    auto color_buffer_attachment_info = vk::RenderingAttachmentInfo{
        .imageView   = swap_chain_.image_views()[image_index],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F),
    };

    auto depth_buffer_attachment_info = vk::RenderingAttachmentInfo{
        .imageView   = swap_chain_.depth_stencil_attachment_image_view(),
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = vk::ClearDepthStencilValue(1.0F, 0),
    };

    if (swap_chain_.msaa_sample_count() != vk::SampleCountFlagBits::e1) {
      // When multisampling is enabled use the color attachment buffer

      transition_color_attachment_layout(TransitionColorAttachmentLayoutInfo{
          .frame_index     = frame_index,
          .old_layout      = vk::ImageLayout::eUndefined,
          .new_layout      = vk::ImageLayout::eColorAttachmentOptimal,
          .src_access_mask = {},
          .dst_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
          .src_stage_mask  = vk::PipelineStageFlagBits2::eTopOfPipe,
          .dst_stage_mask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      });
      color_buffer_attachment_info.imageView          = swap_chain_.color_attachment_image_view();
      color_buffer_attachment_info.resolveMode        = vk::ResolveModeFlagBits::eAverage;
      color_buffer_attachment_info.resolveImageView   = swap_chain_.image_views()[image_index];
      color_buffer_attachment_info.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    }

    auto rendering_info = vk::RenderingInfo{
        // Defines the size of the render area
        .renderArea =
            vk::Rect2D{
                .offset = {.x = 0, .y = 0}, .extent = swap_chain_.extent()  //
            },
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &color_buffer_attachment_info,
        .pDepthAttachment     = &depth_buffer_attachment_info,
    };
    graphics_command_buffers_[frame_index].beginRendering(rendering_info);

    graphics_command_buffers_[frame_index].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    graphics_command_buffers_[frame_index].setViewport(
        0, vk::Viewport{
               .x      = 0.0F,
               .y      = 0.0F,
               .width  = static_cast<float>(swap_chain_.extent().width),
               .height = static_cast<float>(swap_chain_.extent().height),
               // Note: min and max depth must be between [0.0F, 1.0F] and min might be higher than max.
               .minDepth = 0.0F,
               .maxDepth = 1.0F  //
           });
    graphics_command_buffers_[frame_index].setScissor(
        0, vk::Rect2D{.offset = vk::Offset2D{.x = 0, .y = 0}, .extent = swap_chain_.extent()});
    graphics_command_buffers_[frame_index].bindVertexBuffers(0, {ssbuffers_[current_frame_].buffer()}, {0});
    graphics_command_buffers_[frame_index].draw(ParticleSystem::kParticleCount, 1, 0, 0);

    graphics_command_buffers_[frame_index].endRendering();

    // Transition the image layout from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
    transition_swap_chain_image_layout(TransitionSwapChainImageLayoutInfo{
        .image_index     = image_index,
        .frame_index     = frame_index,
        .old_layout      = vk::ImageLayout::eColorAttachmentOptimal,
        .new_layout      = vk::ImageLayout::ePresentSrcKHR,
        .src_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dst_access_mask = {},
        .src_stage_mask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dst_stage_mask  = vk::PipelineStageFlagBits2::eBottomOfPipe,
    });

    graphics_command_buffers_[frame_index].end();
  }

  void record_compute_command_buffer(size_t frame_index) {
    compute_command_buffers_[frame_index].reset();
    compute_command_buffers_[frame_index].begin({});
    compute_command_buffers_[frame_index].bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline_);
    compute_command_buffers_[frame_index].bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline_layout_,
                                                             0, {compute_descriptor_sets_[frame_index]}, {});
    compute_command_buffers_[frame_index].dispatch(ParticleSystem::kParticleCount / 256, 1, 1);
    compute_command_buffers_[frame_index].end();
  }

  void create_descriptor_pool() {
    auto pool_size = std::array{
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, kMaxFramesInFlight),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, kMaxFramesInFlight * 2),
    };
    auto pool_info = vk::DescriptorPoolCreateInfo{
        .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets       = kMaxFramesInFlight,
        .poolSizeCount = pool_size.size(),
        .pPoolSizes    = pool_size.data(),
    };
    descriptor_pool_ =
        vkren::Result(device_->createDescriptorPool(pool_info)).or_panic("Could not create descriptor pool");
  }

  void create_descriptor_set_layout() {
    auto bindings = std::array{
        vk::DescriptorSetLayoutBinding{
            .binding            = 0,
            .descriptorType     = vk::DescriptorType::eUniformBuffer,
            .descriptorCount    = 1,
            .stageFlags         = vk::ShaderStageFlagBits::eCompute,
            .pImmutableSamplers = nullptr,
        },
        vk::DescriptorSetLayoutBinding{
            .binding            = 1,
            .descriptorType     = vk::DescriptorType::eStorageBuffer,
            .descriptorCount    = 1,
            .stageFlags         = vk::ShaderStageFlagBits::eCompute,
            .pImmutableSamplers = nullptr,
        },
        vk::DescriptorSetLayoutBinding{
            .binding            = 2,
            .descriptorType     = vk::DescriptorType::eStorageBuffer,
            .descriptorCount    = 1,
            .stageFlags         = vk::ShaderStageFlagBits::eCompute,
            .pImmutableSamplers = nullptr,
        },
    };
    auto layout_info = vk::DescriptorSetLayoutCreateInfo{
        .bindingCount = bindings.size(),
        .pBindings    = bindings.data(),
    };
    compute_descriptor_set_layout_ = vkren::Result(device_->createDescriptorSetLayout(layout_info)).or_panic();
  }

  void create_descriptor_sets() {
    compute_descriptor_sets_.clear();

    auto layouts         = std::vector<vk::DescriptorSetLayout>(kMaxFramesInFlight, compute_descriptor_set_layout_);
    auto desc_alloc_info = vk::DescriptorSetAllocateInfo{
        .descriptorPool     = descriptor_pool_,
        .descriptorSetCount = kMaxFramesInFlight,
        .pSetLayouts        = layouts.data(),
    };
    compute_descriptor_sets_ =
        vkren::Result(device_->allocateDescriptorSets(desc_alloc_info)).or_panic("Could not allocate descriptor sets");

    for (auto i = 0U; i < kMaxFramesInFlight; ++i) {
      auto buffer_info = vk::DescriptorBufferInfo{
          .buffer = uniform_buffers_[i].buffer(),
          .offset = 0,
          .range  = sizeof(UniformBufferObject),
      };

      auto last_ind           = (i - 1) % kMaxFramesInFlight;
      auto last_frame_ss_info = vk::DescriptorBufferInfo{
          .buffer = ssbuffers_[last_ind].buffer(),
          .offset = 0,
          .range  = sizeof(Particle) * ParticleSystem::kParticleCount,
      };
      auto curr_ind              = i;
      auto current_frame_ss_info = vk::DescriptorBufferInfo{
          .buffer = ssbuffers_[curr_ind].buffer(),
          .offset = 0,
          .range  = sizeof(Particle) * ParticleSystem::kParticleCount,
      };

      auto desc_writes = std::array{
          vk::WriteDescriptorSet{
              .dstSet           = *compute_descriptor_sets_[i],
              .dstBinding       = 0,
              .dstArrayElement  = 0,
              .descriptorCount  = 1,
              .descriptorType   = vk::DescriptorType::eUniformBuffer,
              .pImageInfo       = nullptr,
              .pBufferInfo      = &buffer_info,
              .pTexelBufferView = nullptr,
          },
          vk::WriteDescriptorSet{
              .dstSet           = *compute_descriptor_sets_[i],
              .dstBinding       = 1,
              .dstArrayElement  = 0,
              .descriptorCount  = 1,
              .descriptorType   = vk::DescriptorType::eStorageBuffer,
              .pImageInfo       = nullptr,
              .pBufferInfo      = &last_frame_ss_info,
              .pTexelBufferView = nullptr,
          },
          vk::WriteDescriptorSet{
              .dstSet           = *compute_descriptor_sets_[i],
              .dstBinding       = 2,
              .dstArrayElement  = 0,
              .descriptorCount  = 1,
              .descriptorType   = vk::DescriptorType::eStorageBuffer,
              .pImageInfo       = nullptr,
              .pBufferInfo      = &current_frame_ss_info,
              .pTexelBufferView = nullptr,
          },
      };
      device_->updateDescriptorSets(desc_writes, {});
    }
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

  static void framebuffer_resize_callback(GLFWwindow* window, int /*width*/, int /*height*/) {
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

 private:
  /**
   * @brief Responsible for dynamic loading of the Vulkan library, it's a starting point for creating other RAII-based
   * Vulkan objects like vk::raii::Instance or vk::raii::Device. e.g. context_.createInstance(...) wraps C-style
   * vkCreateInstance.
   *
   */
  vk::raii::Context context_;

  vkren::Device device_        = vkren::Device(nullptr);
  vkren::SwapChain swap_chain_ = vkren::SwapChain(nullptr);

  /**
   * @brief Describes the uniform buffers used in shaders.
   *
   */
  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::PipelineLayout compute_pipeline_layout_  = nullptr;

  /**
   * @brief Descriptor set layout object is defined by an array of zero or more descriptor bindings. It's a way for
   * shaders to freely access resource like buffers and images
   *
   */
  vk::raii::DescriptorSetLayout compute_descriptor_set_layout_ = nullptr;

  /**
   * @brief Describes the graphics pipeline, including shaders stages, input assembly, rasterization and more.
   *
   */
  vk::raii::Pipeline graphics_pipeline_ = nullptr;
  vk::raii::Pipeline compute_pipeline_  = nullptr;

  /**
   * @brief Command pools manage the memory that is used to store the buffers and command buffers are allocated from
   * them.
   *
   */
  vk::raii::CommandPool command_pool_ = nullptr;

  uint32_t current_frame_ = 0;

  // Multiple frames are created in flight at once. Rendering of one frame does not interfere with the recording of
  // the other. We choose the number 2, because we don't want the CPU to go to far ahead of the GPU.
  static constexpr int kMaxFramesInFlight = 2;

  /**
   * @brief Drawing operations are recorded in comand buffer objects.
   *
   */
  std::vector<vk::raii::CommandBuffer> graphics_command_buffers_;
  std::vector<vk::raii::CommandBuffer> compute_command_buffers_;

  /**
   * @brief Semaphores are used to assert on GPU that a process e.g. rendering is finished.
   *
   */
  vk::raii::Semaphore timeline_semaphore_ = nullptr;
  uint64_t timeline_value_{0};

  /**
   * @brief Fences are used to block GPU until the frame is presented.
   *
   */
  std::vector<vk::raii::Fence> in_flight_fences_;

  std::vector<eray::vkren::ExclusiveBufferResource> uniform_buffers_;
  std::vector<void*> uniform_buffers_mapped_;

  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
  std::vector<vk::raii::DescriptorSet> compute_descriptor_sets_;

  vk::raii::ImageView txt_view_  = nullptr;
  vk::raii::Sampler txt_sampler_ = nullptr;

  std::vector<vkren::ExclusiveBufferResource> ssbuffers_;

  /**
   * @brief GLFW window pointer.
   *
   */
  GLFWwindow* window_ = nullptr;

  /**
   * @brief Although many drivers and platforms trigger VK_ERROR_OUT_OF_DATE_KHR automatically after a window resize,
   * it is not guaranteed to happen. That's why there is an extra code to handle resizes explicitly.
   *
   */
  bool framebuffer_resized_ = false;

  float last_frame_time_{};

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

  static constexpr eray::util::zstring_view kComputeShaderEntryPoint  = "mainComp";
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
