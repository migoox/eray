#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <depth_buffer/vertex.hpp>
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
#include <liberay/vkren/descriptor.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/glfw/vk_glfw_window_creator.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/pipeline.hpp>
#include <liberay/vkren/shader.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <ranges>
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

class DepthBufferApplication {
 public:
  DepthBufferApplication() = default;

  void run() {
    window_ = eray::os::System::instance().create_window().or_panic("Could not create a window");
    init_vk();
    main_loop();
    cleanup();
  }

  static constexpr uint32_t kWinWidth  = 800;
  static constexpr uint32_t kWinHeight = 600;

 private:
  void init_vk() {
    create_device();
    create_swap_chain();
    create_buffers();
    create_txt_img();
    create_descriptors();
    create_graphics_pipeline();
    create_command_pool();
    create_command_buffers();
    create_sync_objs();
  }

  void create_device() {
    auto desktop_profile                  = vkren::Device::CreateInfo::DesktopProfile{};
    auto device_info                      = desktop_profile.get(*window_);
    device_info.app_info.pApplicationName = "Compute Particles Example";
    device_ = vkren::Device::create(context_, device_info).or_panic("Could not create a logical device wrapper");
  }

  void main_loop() {
    while (!window_->should_close()) {
      window_->poll_events();
      draw_frame();
    }

    // Since draw frame operations are async, when the main loop ends the drawing operations may still be going on.
    // This call is allows for the async operations to finish before cleaning the resources.
    device_->waitIdle();
  }

  void draw_frame() {
    // A binary (there is also a timeline semaphore) semaphore is used to add order between queue operations (work
    // submitted to the queue). Semaphores are used for both -- to order work inside the same queue and between
    // different queues. The waiting happens on GPU only, the host (CPU) is not blocked.
    //
    // A fence is used on CPU. Unlike the semaphores the vkWaitForFence is blocking the host.

    while (vk::Result::eTimeout == device_->waitForFences(*in_flight_fences_[current_frame_], vk::True, UINT64_MAX)) {
      ;
    }

    // Get the image from the swap chain. When the image will be ready the present semaphore will be signaled.
    uint32_t image_index{};
    if (auto acquire_opt =
            swap_chain_.acquire_next_image(UINT64_MAX, *present_finished_semaphores_[current_semaphore_], nullptr)) {
      if (acquire_opt->status != vkren::SwapChain::AcquireResult::Status::Success) {
        return;
      }
      image_index = acquire_opt->image_index;
    } else {
      eray::util::panic("Failed to acquire next image!");
      return;
    }

    update_ubo(current_frame_);

    device_->resetFences(*in_flight_fences_[current_frame_]);
    graphics_command_buffers_[current_frame_].reset();
    record_graphics_command_buffer(current_frame_, image_index);

    auto wait_dst_stage_mask = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const auto submit_info   = vk::SubmitInfo{
          .waitSemaphoreCount   = 1,
          .pWaitSemaphores      = &*present_finished_semaphores_[current_semaphore_],
          .pWaitDstStageMask    = &wait_dst_stage_mask,
          .commandBufferCount   = 1,
          .pCommandBuffers      = &*graphics_command_buffers_[current_frame_],
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

    if (!swap_chain_.present_image(present_info)) {
      eray::util::Logger::err("Failed to present an image!");
    }

    current_semaphore_ = (current_semaphore_ + 1) % present_finished_semaphores_.size();
    current_frame_     = (current_frame_ + 1) % kMaxFramesInFlight;
  }

  void create_descriptors() {
    dsl_manager_   = vkren::DescriptorSetLayoutManager::create(device_);
    auto ratios    = vkren::DescriptorPoolSizeRatio::create_default();
    dsl_allocator_ = vkren::DescriptorAllocator::create_and_init(device_, 100, ratios).or_panic();
    auto result    = vkren::DescriptorSetBuilder::create(dsl_manager_, dsl_allocator_)
                      .with_binding(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
                      .with_binding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                      .build_many(kMaxFramesInFlight)
                      .or_panic("Could not create descriptor sets");

    descriptor_sets_ = result.descriptor_sets;
    dsl_             = result.layout;

    auto writer = vkren::DescriptorSetWriter::create(device_);
    for (auto i = 0U; i < kMaxFramesInFlight; ++i) {
      writer.write_buffer(0, uniform_buffers_[i].desc_buffer_info(), vk::DescriptorType::eUniformBuffer);
      writer.write_combined_image_sampler(1, txt_view_, txt_sampler_, vk::ImageLayout::eShaderReadOnlyOptimal);
      writer.write_to_set(descriptor_sets_[i]);
      writer.clear();
    }
  }

  void update_ubo(uint32_t image_index) {
    static auto start_time = std::chrono::high_resolution_clock::now();
    auto curr_time         = std::chrono::high_resolution_clock::now();
    auto time              = std::chrono::duration<float>(curr_time - start_time).count();

    auto s = std::sin(time * 0.7F);
    s      = (s * s - 0.5F) * 90.F;
    UniformBufferObject ubo{};
    ubo.model = eray::math::rotation_axis(eray::math::radians(s), eray::math::Vec3f(0.F, 1.F, 0.F));
    ubo.view  = eray::math::translation(eray::math::Vec3f(0.F, 0.F, -4.F));
    ubo.proj  = eray::math::perspective_vk_rh(
        eray::math::radians(80.0F), static_cast<float>(kWinWidth) / static_cast<float>(kWinHeight), 0.01F, 10.F);

    memcpy(uniform_buffers_mapped_[image_index], &ubo, sizeof(ubo));
  }

  void cleanup() {
    swap_chain_.destroy();

    eray::util::Logger::succ("Finished cleanup");
  }

  void create_swap_chain() {
    swap_chain_ = vkren::SwapChain::create(device_, window_, device_.max_usable_sample_count())
                      .or_panic("Could not create a swap chain");
  }

  void create_graphics_pipeline() {
    auto main_binary =
        eray::res::SPIRVShaderBinary::load_from_path(eray::os::System::executable_dir() / "shaders" / "main.spv")
            .or_panic("Could not find main_sh.spv");
    auto main_shader_module =
        vkren::ShaderModule::create(device_, main_binary).or_panic("Could not create a main shader module");

    auto binding_desc = Vertex::binding_desc();
    auto attribs_desc = Vertex::attribs_desc();

    auto pipeline = vkren::GraphicsPipelineBuilder::create(swap_chain_)
                        .with_shaders(main_shader_module.shader_module, main_shader_module.shader_module)
                        .with_polygon_mode(vk::PolygonMode::eFill)
                        .with_cull_mode(vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise)
                        .with_input_state(binding_desc, attribs_desc)
                        .with_descriptor_set_layout(dsl_)
                        .with_primitive_topology(vk::PrimitiveTopology::eTriangleList)
                        .with_depth_test()
                        .with_depth_test_compare_op(vk::CompareOp::eLess)
                        .with_blending()
                        .build(device_)
                        .or_panic("Could not create a graphics pipeline");

    graphics_pipeline_        = std::move(pipeline.pipeline);
    graphics_pipeline_layout_ = std::move(pipeline.layout);
  }

  void create_command_pool() {
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

    command_pool_ =
        vkren::Result(device_->createCommandPool(command_pool_info)).or_panic("Could not create a command pool");
  }

  void create_buffers() {
    auto vb = VertexBuffer::create();

    auto vertices_region = eray::util::MemoryRegion{vb.vertices.data(), vb.vertices_size_bytes()};
    vert_buffer_         = vkren::BufferResource::create_vertex_buffer(device_, vertices_region.size_bytes())
                       .or_panic("Could not create the vertex buffer");
    vert_buffer_.write(vertices_region).or_panic("Could not fill the vertex buffer");

    auto indices_region = eray::util::MemoryRegion{vb.indices.data(), vb.indices_size_bytes()};
    ind_buffer_         = vkren::BufferResource::create_index_buffer(device_, indices_region.size_bytes())
                      .or_panic("Could not create a Vertex Buffer");
    ind_buffer_.write(indices_region).or_panic("Could not fill the index buffer");

    // Copying to uniform buffer each frame means that staging buffer makes no sense.
    // We should have multiple buffers, because multiple frames may be in flight at the same time and
    // we don’t want to update the buffer in preparation of the next frame while a previous one is still reading
    // from it!

    uniform_buffers_.clear();
    uniform_buffers_mapped_.clear();
    vk::DeviceSize size_bytes = sizeof(UniformBufferObject);
    for (auto i = 0; i < kMaxFramesInFlight; ++i) {
      auto ubo = vkren::BufferResource::create_persistently_mapped_uniform_buffer(device_, size_bytes)
                     .or_panic("Could not create the uniform buffer");
      uniform_buffers_.emplace_back(std::move(ubo.buffer));
      uniform_buffers_mapped_.emplace_back(ubo.mapped_data);
    }
  }

  void create_command_buffers() {
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

    auto result =
        vkren::Result(device_->allocateCommandBuffers(alloc_info)).or_panic("Could not allocate a command buffer");
    std::ranges::move(result | std::views::take(kMaxFramesInFlight), graphics_command_buffers_.begin());
  }

  void create_sync_objs() {
    present_finished_semaphores_.clear();
    render_finished_semaphores_.clear();

    for (size_t i = 0; i < swap_chain_.images().size(); ++i) {
      if (auto result = device_->createSemaphore(vk::SemaphoreCreateInfo{})) {
        present_finished_semaphores_.emplace_back(std::move(*result));
      } else {
        eray::util::panic("Could not create a semaphore");
      }
    }

    for (size_t i = 0; i < swap_chain_.images().size(); ++i) {
      if (auto result = device_->createSemaphore(vk::SemaphoreCreateInfo{})) {
        render_finished_semaphores_.emplace_back(std::move(*result));
      } else {
        eray::util::panic("Could not create a semaphore");
      }
    }

    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
      if (auto result = device_->createFence(vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled})) {
        in_flight_fences_[i] = std::move(*result);
      } else {
        eray::util::panic("Could not create a fence");
      }
    }
  }

  void create_txt_img() {
    auto img = eray::res::Image::load_from_path(eray::os::System::executable_dir() / "assets" / "cad.jpeg")
                   .or_panic("cad is not there :(");
    // Image
    txt_image_ = vkren::ImageResource::create_texture(device_, vkren::ImageDescription::from(img))
                     .or_panic("Could not create a texture image");
    txt_image_.upload(img.memory_region()).or_panic("Could not upload the image");
    txt_view_ = txt_image_.create_image_view().or_panic("Could not create the image view");

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
        .minLod           = 0.F,
        .maxLod           = vk::LodClampNone,
    };
    txt_sampler_ = vkren::Result(device_->createSampler(sampler_info)).or_panic("Could not create the sampler");
  }

  /**
   * @brief Writes the commands we what to execute into a command buffer
   *
   * @param image_index
   */
  void record_graphics_command_buffer(size_t frame_index, uint32_t image_index) {
    swap_chain_.begin_rendering(graphics_command_buffers_[frame_index], image_index,
                                vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F), vk::ClearDepthStencilValue(1.0F, 0));

    // We can specify type of the pipeline
    graphics_command_buffers_[frame_index].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    graphics_command_buffers_[frame_index].bindVertexBuffers(0, vert_buffer_.buffer(), {0});
    graphics_command_buffers_[frame_index].bindIndexBuffer(ind_buffer_.buffer(), 0, vk::IndexType::eUint16);

    // Describes the region of framebuffer that the output will be rendered to
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

    // Scissor rectangle defines in which region pixels will actually be stored. The rasterizer will discard any
    // pixels outside the scissored rectangle. We want to draw to entire framebuffer.
    graphics_command_buffers_[frame_index].setScissor(
        0, vk::Rect2D{.offset = vk::Offset2D{.x = 0, .y = 0}, .extent = swap_chain_.extent()});

    // Unlike vertex and index buffers, descriptor sets are not unique to graphics pipelines. Therefore, we need
    // to specify if we want to bind descriptor sets to the graphics or compute pipeline. The next parameter is the
    // layout that the descriptors are based on. The next three parameters specify the index of the first descriptor
    // set, the number of sets to bind and the array of sets to bind.
    graphics_command_buffers_[frame_index].bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, graphics_pipeline_layout_, 0, descriptor_sets_[frame_index], nullptr);

    graphics_command_buffers_[frame_index].drawIndexed(12, 1, 0, 0, 0);

    swap_chain_.end_rendering(graphics_command_buffers_[frame_index], image_index);
  }

  // Multiple frames are created in flight at once. Rendering of one frame does not interfere with the recording of
  // the other. We choose the number 2, because we don't want the CPU to go to far ahead of the GPU.
  static constexpr int kMaxFramesInFlight = 2;

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

  /**
   * @brief Drawing operations are recorded in comand buffer objects.
   *
   */
  std::array<vk::raii::CommandBuffer, kMaxFramesInFlight> graphics_command_buffers_ = {nullptr, nullptr};

  /**
   * @brief Semaphores are used to assert on GPU that a process e.g. rendering is finished.
   *
   */
  std::vector<vk::raii::Semaphore> present_finished_semaphores_;
  std::vector<vk::raii::Semaphore> render_finished_semaphores_;

  /**
   * @brief Fences are used to block GPU until the frame is presented.
   *
   */
  std::array<vk::raii::Fence, kMaxFramesInFlight> in_flight_fences_ = {nullptr, nullptr};

  vkren::BufferResource vert_buffer_;
  vkren::BufferResource ind_buffer_;

  std::vector<vkren::BufferResource> uniform_buffers_;
  std::vector<void*> uniform_buffers_mapped_;

  vkren::ImageResource txt_image_;
  vk::raii::ImageView txt_view_  = nullptr;
  vk::raii::Sampler txt_sampler_ = nullptr;

  vkren::DescriptorSetLayoutManager dsl_manager_ = vkren::DescriptorSetLayoutManager(nullptr);
  vkren::DescriptorAllocator dsl_allocator_      = vkren::DescriptorAllocator(nullptr);

  std::shared_ptr<eray::os::Window> window_ = nullptr;
  std::vector<vk::DescriptorSet> descriptor_sets_;
  vk::DescriptorSetLayout dsl_;
};

int main() {
  using Logger = eray::util::Logger;
  using System = eray::os::System;

  Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());
  Logger::instance().set_abs_build_path(ERAY_BUILD_ABS_PATH);

  auto window_creator =
      eray::os::VulkanGLFWWindowCreator::create().or_panic("Could not create a Vulkan GLFW window creator");
  System::init(std::move(window_creator)).or_panic("Could not initialize Operating System API");

  auto app = DepthBufferApplication();
  app.run();

  eray::os::System::instance().terminate();

  return 0;
}
