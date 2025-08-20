#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/system.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <ranges>
#include <sandbox/vertex.hpp>
#include <variant>
#include <vector>
#include <version/version.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include "liberay/vkren/error.hpp"

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
  std::expected<void, AppError> run() {
    TRY(initWindow());
    TRY(init_vk())
    mainLoop();
    cleanup();

    return {};
  }

  static constexpr uint32_t kWinWidth  = 800;
  static constexpr uint32_t kWinHeight = 600;

 private:
  std::expected<void, VulkanInitError> init_vk() {
    TRY(create_device())
    TRY(create_swap_chain())
    create_descriptor_set_layout();
    TRY(create_graphics_pipeline())
    TRY(create_command_pool())
    TRY(create_buffers())
    TRY(create_command_buffers())
    create_txt_img();
    create_descriptor_pool();
    create_descriptor_sets();
    TRY(create_sync_objs())

    return {};
  }

  std::expected<void, VulkanInitError> create_device() {
    // == Global Extensions ============================================================================================
    auto required_global_extensions = std::vector<const char*>();
    {
      uint32_t glfw_extensions_count = 0;
      auto* glfw_extensions          = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);
      required_global_extensions = std::vector<const char*>(glfw_extensions, glfw_extensions + glfw_extensions_count);
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
    auto desktop_template                 = vkren::Device::CreateInfo::DesktopTemplate();
    auto device_info                      = desktop_template.get(surface_creator, required_global_extensions);
    device_info.app_info.pApplicationName = "VkTriangle";

    if (auto result = vkren::Device::create(context_, device_info)) {
      device_ = std::move(*result);
    } else {
      eray::util::Logger::err("Could not create a logical device wrapper");
      return std::unexpected(VulkanObjectCreationError{});
    }

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
    device_->waitIdle();
  }

  std::expected<void, DrawFrameError> draw_frame() {
    // A binary (there is also a timeline semaphore) semaphore is used to add order between queue operations (work
    // submitted to the queue). Semaphores are used for both -- to order work inside the same queue and between
    // different queues. The waiting happens on GPU only, the host (CPU) is not blocked.
    //
    // A fence is used on CPU. Unlike the semaphores the vkWaitForFence is blocking the host.

    while (vk::Result::eTimeout == device_->waitForFences(*in_flight_fences_[current_frame_], vk::True, UINT64_MAX)) {
      ;
    }

    // Get the image from the swap chain. When the image will be ready the present semaphore will be signaled.
    auto [result, image_index] =
        swap_chain_->acquireNextImage(UINT64_MAX, *present_complete_semaphores_[current_semaphore_], nullptr);

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
    update_ubo(current_frame_);

    device_->resetFences(*in_flight_fences_[current_frame_]);
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
    device_.graphics_queue().submit(submit_info, *in_flight_fences_[current_frame_]);

    // The image will not be presented until the render finished semaphore is signaled by the submit call.
    const auto present_info = vk::PresentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &*render_finished_semaphores_[image_index],
        .swapchainCount     = 1,
        .pSwapchains        = &**swap_chain_,
        .pImageIndices      = &image_index,
        .pResults           = nullptr,
    };

    result = device_.presentation_queue().presentKHR(present_info);
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

  void create_descriptor_set_layout() {
    auto bindings = std::array{
        // Uniform buffer
        vk::DescriptorSetLayoutBinding{
            .binding        = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,

            // Single uniform buffer contains one MVP
            .descriptorCount = 1,

            // We only reference the descriptor from the vertex shader
            .stageFlags = vk::ShaderStageFlagBits::eVertex,

            // The descriptor is not related to image sampling
            .pImmutableSamplers = nullptr,
        },

        // Sampler
        vk::DescriptorSetLayoutBinding{
            .binding            = 1,
            .descriptorType     = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount    = 1,
            .stageFlags         = vk::ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = nullptr,
        },
    };
    auto layout_info = vk::DescriptorSetLayoutCreateInfo{
        .bindingCount = bindings.size(),
        .pBindings    = bindings.data(),
    };
    descriptor_set_layout_ = vkren::Result(device_->createDescriptorSetLayout(layout_info)).or_panic();
  }

  void update_ubo(uint32_t image_index) {
    // static auto start_time = std::chrono::high_resolution_clock::now();

    // auto curr_time = std::chrono::high_resolution_clock::now();
    // float time     = std::chrono::duration<float, std::chrono::seconds::period>(curr_time - start_time).count();

    UniformBufferObject ubo{};

    // right-handed, depth [0, 1]
    ubo.model = eray::math::translation(eray::math::Vec3f(0.F, 0.F, 0.F)) *
                eray::math::rotation_axis(eray::math::radians(45.0F), eray::math::Vec3f(0.F, 0.F, 1.F)) *
                eray::math::rotation_axis(eray::math::radians(-50.0F), eray::math::Vec3f(1.F, 0.F, 0.F));
    ubo.view = eray::math::translation(eray::math::Vec3f(0.F, 0.F, -4.F));
    ubo.proj = eray::math::perspective_vk_rh(
        eray::math::radians(80.0F), static_cast<float>(kWinWidth) / static_cast<float>(kWinHeight), 0.01F, 10.F);

    memcpy(uniform_buffers_mapped_[image_index], &ubo, sizeof(ubo));
  }

  void cleanup() {
    swap_chain_.cleanup();

    glfwDestroyWindow(window_);
    glfwTerminate();

    eray::util::Logger::succ("Finished cleanup");
  }

  std::expected<void, VulkanInitError> create_swap_chain() {
    // Unfortunately, if you are using a high DPI display (like Apple’s Retina display), screen coordinates don’t
    // correspond to pixels. For that reason we use glfwGetFrameBufferSize to get size in pixels. (Note:
    // glfwGetWindowSize returns size in screen coordinates).
    int width{};
    int height{};
    glfwGetFramebufferSize(window_, &width, &height);

    if (auto result = vkren::SwapChain::create(device_, static_cast<uint32_t>(width), static_cast<uint32_t>(height))) {
      swap_chain_ = std::move(*result);
    } else {
      eray::util::Logger::err("Could not create a swap chain");
      return std::unexpected(VulkanObjectCreationError{});
    }

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

    if (!swap_chain_.recreate(device_, static_cast<uint32_t>(width), static_cast<uint32_t>(height))) {
      return std::unexpected(SwapchainRecreationFailure{});
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
    auto binding_desc       = Vertex::get_binding_desc();
    auto attribs_desc       = Vertex::get_attribs_desc();
    auto vertex_input_state = vk::PipelineVertexInputStateCreateInfo{
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &binding_desc,  //
        .vertexAttributeDescriptionCount = attribs_desc.size(),
        .pVertexAttributeDescriptions    = attribs_desc.data(),  //
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
    auto depth_stencil_state_info = vk::PipelineDepthStencilStateCreateInfo{
        .depthTestEnable       = vk::True,
        .depthWriteEnable      = vk::True,
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

    // You can use uniform values in shaders, which are globals that can be changed at drawing time after the behavior
    // of your shaders without having to recreate. The uniform variables must be specified during the pipeline creation.
    auto pipeline_layout_info = vk::PipelineLayoutCreateInfo{
        .setLayoutCount         = 1,
        .pSetLayouts            = &*descriptor_set_layout_,
        .pushConstantRangeCount = 0,
    };

    if (auto result = device_->createPipelineLayout(pipeline_layout_info)) {
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
    auto format = swap_chain_.color_format();
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info{
        .colorAttachmentCount    = 1,
        .pColorAttachmentFormats = &format,
        .depthAttachmentFormat   = swap_chain_.depth_stencil_format(),
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
        .layout              = pipeline_layout_,

        .renderPass = nullptr,  // we are using dynamic rendering

        // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex  = -1,
    };

    // Pipeline cache (set to nullptr) can be used to store and reuse data relevant to pipeline creation across
    // multiple calls to vk::CreateGraphicsPipelines and even across program executions if the cache is stored to a
    // file.
    if (auto result = device_->createGraphicsPipeline(nullptr, pipeline_info)) {
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
        .queueFamilyIndex = device_.graphics_queue_family(),
    };

    if (auto result = device_->createCommandPool(command_pool_info)) {
      command_pool_ = std::move(*result);
    } else {
      eray::util::Logger::err("Could not create a command pool. {}", vk::to_string(result.error()));

      return std::unexpected(VulkanObjectCreationError{
          .result = result.error(),
      });
    }

    return {};
  }

  std::expected<void, VulkanInitError> create_buffers() {
    auto vb = VertexBuffer::create_triangle();

    // Vertex buffer
    {
      vk::DeviceSize buffer_size = vb.vertices_size_in_bytes();
      auto staging_buffer        = vkren::ExclusiveBufferResource::create(  //
                                device_,
                                vkren::ExclusiveBufferResource::CreateInfo{
                                           .size_in_bytes = buffer_size,
                                           .buff_usage    = vk::BufferUsageFlagBits::eTransferSrc,
                                })
                                       .or_panic();
      staging_buffer.fill_data(vb.vertices.data(), 0, vb.vertices_size_in_bytes());

      vert_buffer_ =
          vkren::ExclusiveBufferResource::create(  //
              device_,
              vkren::ExclusiveBufferResource::CreateInfo{
                  .size_in_bytes = buffer_size,
                  .buff_usage    = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,

                  // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT bit specifies that memory allocated with this type is
                  // the most efficient for device access. The memory mapping is not allowed!
                  .mem_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
              })
              .or_panic();
      vert_buffer_.copy_from(device_, staging_buffer.buffer, vk::BufferCopy(0, 0, buffer_size));
    }

    // Index buffer
    {
      vk::DeviceSize buffer_size = vb.indices_size_in_bytes();
      auto staging_buffer        = vkren::ExclusiveBufferResource::create(  //
                                device_,
                                vkren::ExclusiveBufferResource::CreateInfo{
                                           .size_in_bytes = buffer_size,
                                           .buff_usage    = vk::BufferUsageFlagBits::eTransferSrc,
                                })
                                       .or_panic();
      staging_buffer.fill_data(vb.indices.data(), 0, vb.indices_size_in_bytes());

      ind_buffer_ = vkren::ExclusiveBufferResource::create(  //
                        device_,
                        vkren::ExclusiveBufferResource::CreateInfo{
                            .size_in_bytes = buffer_size,
                            .buff_usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                            .mem_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                        })
                        .or_panic();
      ind_buffer_.copy_from(device_, staging_buffer.buffer, vk::BufferCopy(0, 0, buffer_size));
    }

    // Copying to uniform buffer each frame means that staging buffer makes no sense.
    // We should have multiple buffers, because multiple frames may be in flight at the same time and
    // we don’t want to update the buffer in preparation of the next frame while a previous one is still reading
    // from it!

    uniform_buffers_.clear();
    uniform_buffers_mapped_.clear();
    {
      vk::DeviceSize buffer_size = sizeof(UniformBufferObject);
      for (auto i = 0; i < kMaxFramesInFlight; ++i) {
        auto ubo = vkren::ExclusiveBufferResource::create(  //
                       device_,
                       vkren::ExclusiveBufferResource::CreateInfo{
                           .size_in_bytes = buffer_size,
                           .buff_usage    = vk::BufferUsageFlagBits::eUniformBuffer,
                           .mem_properties =
                               vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                       })
                       .or_panic();

        // This technique is called persistent mapping, the buffer stays mapped for the application's whole life-time.
        // It increases performance as the mapping process is not free.
        uniform_buffers_mapped_.emplace_back(ubo.memory.mapMemory(0, buffer_size));
        uniform_buffers_.emplace_back(std::move(ubo));
      }
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

    if (auto result = device_->allocateCommandBuffers(alloc_info)) {
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

    for (size_t i = 0; i < swap_chain_.images().size(); ++i) {
      if (auto result = device_->createSemaphore(vk::SemaphoreCreateInfo{})) {
        present_complete_semaphores_.emplace_back(std::move(*result));
      } else {
        eray::util::Logger::err("Failed to create a  sempahore. {}", vk::to_string(result.error()));
        return std::unexpected(VulkanObjectCreationError{.result = result.error()});
      }
    }

    for (size_t i = 0; i < swap_chain_.images().size(); ++i) {
      if (auto result = device_->createSemaphore(vk::SemaphoreCreateInfo{})) {
        render_finished_semaphores_.emplace_back(std::move(*result));
      } else {
        eray::util::Logger::err("Failed to create a sempahore. {}", vk::to_string(result.error()));
        return std::unexpected(VulkanObjectCreationError{.result = result.error()});
      }
    }

    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
      if (auto result = device_->createFence(vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled})) {
        in_flight_fences_[i] = std::move(*result);
      } else {
        eray::util::Logger::err("Failed to create a fence. {}", vk::to_string(result.error()));
        return std::unexpected(VulkanObjectCreationError{.result = result.error()});
      }
    }

    return {};
  }

  void create_txt_img() {
    auto img = eray::res::Image::load_from_path(eray::os::System::executable_dir() / "assets" / "cad.jpeg")
                   .or_panic("cad is not there :(");

    // Staging buffer
    vk::DeviceSize img_size = img.size_in_bytes();
    auto staging_buffer =
        vkren::ExclusiveBufferResource::create(  //
            device_,
            vkren::ExclusiveBufferResource::CreateInfo{
                .size_in_bytes  = img_size,
                .buff_usage     = vk::BufferUsageFlagBits::eTransferSrc,
                .mem_properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            })
            .or_panic("Could not create image staging buffer");
    staging_buffer.fill_data(img.raw(), 0, img_size);

    // Image object
    txt_image_ = vkren::ExclusiveImage2DResource::create(
                     device_,
                     vkren::ExclusiveImage2DResource::CreateInfo{
                         .size_in_bytes = img_size,

                         // We want to sample the image in the fragment shader
                         .image_usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,

                         .format = vk::Format::eR8G8B8A8Srgb,
                         .width  = static_cast<uint32_t>(img.width()),
                         .height = static_cast<uint32_t>(img.height()),

                         // Texels are laid out in an implementation defined order for optimal access
                         .tiling = vk::ImageTiling::eOptimal,

                         .mem_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                     })
                     .or_panic("Could not create an image resource");
    device_.transition_image_layout(txt_image_.image, vk::ImageLayout::eUndefined,
                                    vk::ImageLayout::eTransferDstOptimal);
    txt_image_.copy_from(device_, staging_buffer.buffer);
    device_.transition_image_layout(txt_image_.image, vk::ImageLayout::eTransferDstOptimal,
                                    vk::ImageLayout::eShaderReadOnlyOptimal);

    // Image View
    txt_view_ = txt_image_.create_img_view(device_).or_panic("Could not create the image view");

    // Image Sampler
    auto pdev_props   = device_.physical_device().getProperties();
    auto sampler_info = vk::SamplerCreateInfo{
        .magFilter        = vk::Filter::eLinear,
        .minFilter        = vk::Filter::eLinear,
        .mipmapMode       = vk::SamplerMipmapMode::eLinear,
        .addressModeU     = vk::SamplerAddressMode::eRepeat,
        .addressModeV     = vk::SamplerAddressMode::eRepeat,
        .addressModeW     = vk::SamplerAddressMode::eRepeat,
        .mipLodBias       = 0.0F,
        .anisotropyEnable = vk::True,
        .maxAnisotropy    = pdev_props.limits.maxSamplerAnisotropy,
        .compareEnable    = vk::False,
        .compareOp        = vk::CompareOp::eAlways,
        .minLod           = 0.0F,
        .maxLod           = 0.0F,
    };
    txt_sampler_ = vkren::Result(device_->createSampler(sampler_info)).or_panic("Could not create the sampler");
  }

  struct TransitionColorAttachmentLayoutInfo {
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

    command_buffers_[info.frame_index].pipelineBarrier2(dependency_info);
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
        .image               = swap_chain_.depth_stencil_image(),  //
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
    transition_color_attachment_layout(TransitionColorAttachmentLayoutInfo{
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
        // Specifies which image to render to
        .imageView = swap_chain_.image_views()[image_index],

        // Specifies the layout the image will be in during rendering
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,

        // Specifies what to do with the image before rendering
        .loadOp = vk::AttachmentLoadOp::eClear,

        // Specifies what to do with the image after rendering
        .storeOp = vk::AttachmentStoreOp::eStore,

        .clearValue = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F),
    };
    auto depth_buffer_attachment_info = vk::RenderingAttachmentInfo{
        .imageView   = swap_chain_.depth_stencil_image_view(),
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = vk::ClearDepthStencilValue(1.0F, 0),
    };

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
    command_buffers_[frame_index].beginRendering(rendering_info);

    // We can specify type of the pipeline
    command_buffers_[frame_index].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    command_buffers_[frame_index].bindVertexBuffers(0, *vert_buffer_.buffer, {0});
    command_buffers_[frame_index].bindIndexBuffer(*ind_buffer_.buffer, 0, vk::IndexType::eUint16);

    // Describes the region of framebuffer that the output will be rendered to
    command_buffers_[frame_index].setViewport(
        0, vk::Viewport{
               .x      = 0.0F,
               .y      = 0.0F,
               .width  = static_cast<float>(swap_chain_.extent().width),
               .height = static_cast<float>(swap_chain_.extent().height),
               // Note: min and max depth must be between [0.0F, 1.0F] and min might be higher than max.
               .minDepth = 0.0F,
               .maxDepth = 1.0F  //
           });

    // Scissor rectangle defines in which region pixels will actually be stored. The rasterizer will discard any
    // pixels outside the scissored rectangle. We want to draw to entire framebuffer.
    command_buffers_[frame_index].setScissor(
        0, vk::Rect2D{.offset = vk::Offset2D{.x = 0, .y = 0}, .extent = swap_chain_.extent()});

    // Unlike vertex and index buffers, descriptor sets are not unique to graphics pipelines. Therefore, we need
    // to specify if we want to bind descriptor sets to the graphics or compute pipeline. The next parameter is the
    // layout that the descriptors are based on. The next three parameters specify the index of the first descriptor
    // set, the number of sets to bind and the array of sets to bind.
    command_buffers_[frame_index].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_, 0,
                                                     *descriptor_sets_[frame_index], nullptr);
    // Draw 3 vertices
    command_buffers_[frame_index].drawIndexed(12, 1, 0, 0, 0);

    command_buffers_[frame_index].endRendering();

    // Transition the image layout from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
    transition_color_attachment_layout(TransitionColorAttachmentLayoutInfo{
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

  void create_descriptor_pool() {
    auto pool_size = std::array{
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, kMaxFramesInFlight),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, kMaxFramesInFlight),
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

  void create_descriptor_sets() {
    auto layouts                   = std::vector<vk::DescriptorSetLayout>(kMaxFramesInFlight, *descriptor_set_layout_);
    auto descriptor_set_alloc_info = vk::DescriptorSetAllocateInfo{
        .descriptorPool     = descriptor_pool_,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts        = layouts.data(),
    };
    descriptor_sets_.clear();
    descriptor_sets_ = vkren::Result(device_->allocateDescriptorSets(descriptor_set_alloc_info))
                           .or_panic("Could not create descriptor sets");

    for (auto i = 0U; i < kMaxFramesInFlight; ++i) {
      auto buffer_info = vk::DescriptorBufferInfo{
          .buffer = uniform_buffers_[i].buffer,
          .offset = 0,
          .range  = sizeof(UniformBufferObject),  // It's also possible to use vk::WholeSize
      };
      auto image_info = vk::DescriptorImageInfo{
          .sampler     = txt_sampler_,
          .imageView   = txt_view_,
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
      };

      auto descriptor_write = std::array{
          vk::WriteDescriptorSet{
              // Specifies the descriptor set to update
              .dstSet = descriptor_sets_[i],

              // Specifies the binding of the descriptor set to update
              .dstBinding = 0,

              // It's possible to update multiple descriptors at once in an array, starting at index dstArrayElement
              .dstArrayElement = 0,

              // Specifies how many descriptor arrays we want to update
              .descriptorCount = 1,

              .descriptorType = vk::DescriptorType::eUniformBuffer,
              .pBufferInfo    = &buffer_info,
          },
          vk::WriteDescriptorSet{
              .dstSet          = descriptor_sets_[i],
              .dstBinding      = 1,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
              .pImageInfo      = &image_info,
          },
      };

      device_->updateDescriptorSets(descriptor_write, {});
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
    auto result = device_->createShaderModule(module_info);
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
  vk::raii::PipelineLayout pipeline_layout_ = nullptr;

  /**
   * @brief Descriptor set layout object is defined by an array of zero or more descriptor bindings. It's a way for
   * shaders to freely access resource like buffers and images
   *
   */
  vk::raii::DescriptorSetLayout descriptor_set_layout_ = nullptr;

  /**
   * @brief Describes the graphics pipeline, including shaders stages, input assembly, rasterization and more.
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

  eray::vkren::ExclusiveBufferResource vert_buffer_;
  eray::vkren::ExclusiveBufferResource ind_buffer_;

  std::vector<eray::vkren::ExclusiveBufferResource> uniform_buffers_;
  std::vector<void*> uniform_buffers_mapped_;

  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
  std::vector<vk::raii::DescriptorSet> descriptor_sets_;

  eray::vkren::ExclusiveImage2DResource txt_image_;
  vk::raii::ImageView txt_view_  = nullptr;
  vk::raii::Sampler txt_sampler_ = nullptr;

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
