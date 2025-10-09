#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <chrono>
#include <compute_particles/particle.hpp>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/window.hpp>
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
#include <liberay/vkren/pipeline.hpp>
#include <liberay/vkren/shader.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace vkren = eray::vkren;

class ComputeParticlesApplication {
 public:
  ComputeParticlesApplication() = default;

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
    create_descriptors();
    create_graphics_pipeline();
    create_compute_pipeline();
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
    static auto prev_time = std::chrono::high_resolution_clock::now();

    while (!window_->should_close()) {
      window_->poll_events();
      draw_frame();
      auto curr_time   = std::chrono::high_resolution_clock::now();
      last_frame_time_ = std::chrono::duration<float, std::chrono::seconds::period>(curr_time - prev_time).count();
      prev_time        = curr_time;
    }

    // Since draw frame operations are async, when the main loop ends the drawing operations may still be going on.
    // This call is allows for the async operations to finish before cleaning the resources.
    device_->waitIdle();
  }

  void draw_frame() {
    uint32_t image_index = 0;
    if (auto acquire_opt = swap_chain_.acquire_next_image(UINT64_MAX, nullptr, *in_flight_fences_[current_frame_])) {
      if (acquire_opt->status != vkren::SwapChain::AcquireResult::Status::Success) {
        return;
      }
      image_index = acquire_opt->image_index;
    } else {
      eray::util::panic("Failed to acquire next image!");
    }

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

      if (!swap_chain_.present_image(present_info)) {
        eray::util::Logger::err("Failed to present an image!");
      }
    }

    current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
  }

  void update_ubo(uint32_t frame_index) {
    auto ubo = UniformBufferObject{
        .delta_time = last_frame_time_,
    };
    memcpy(uniform_buffers_mapped_[frame_index], &ubo, sizeof(ubo));
  }

  void cleanup() { swap_chain_.destroy(); }

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

    auto binding_desc = ParticleSystem::binding_desc();
    auto attribs_desc = ParticleSystem::attribs_desc();

    auto pipeline = vkren::GraphicsPipelineBuilder::create(swap_chain_)
                        .with_shaders(main_shader_module.shader_module, main_shader_module.shader_module)
                        .with_polygon_mode(vk::PolygonMode::eFill)
                        .with_cull_mode(vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise)
                        .with_input_state(binding_desc, attribs_desc)
                        .with_primitive_topology(vk::PrimitiveTopology::ePointList)
                        .build(device_)
                        .or_panic("Could not create a graphics pipeline");

    graphics_pipeline_        = std::move(pipeline.pipeline);
    graphics_pipeline_layout_ = std::move(pipeline.layout);
  }

  void create_compute_pipeline() {
    auto particle_binary =
        eray::res::SPIRVShaderBinary::load_from_path(eray::os::System::executable_dir() / "shaders" / "particle.spv")
            .or_panic("Could not find particle compute shader");
    auto particle_shader_module =
        vkren::ShaderModule::create(device_, particle_binary).or_panic("Could not create a main shader module");

    auto pipeline = vkren::ComputePipelineBuilder::create()
                        .with_descriptor_set_layout(compute_descriptor_set_layout_)
                        .with_shader(particle_shader_module.shader_module)
                        .build(device_)
                        .or_panic("Could not create a compute pipeline");

    compute_pipeline_        = std::move(pipeline.pipeline);
    compute_pipeline_layout_ = std::move(pipeline.layout);
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
    auto staging_buff =
        vkren::BufferResource::create_staging_buffer(device_, region).or_panic("Could not create a Staging Buffer");

    for (auto i = 0U; i < kMaxFramesInFlight; ++i) {
      auto temp = vkren::BufferResource::create_gpu_local_buffer(device_, region.size_bytes(),
                                                                 vk::BufferUsageFlagBits::eVertexBuffer |
                                                                     vk::BufferUsageFlagBits::eStorageBuffer |
                                                                     vk::BufferUsageFlagBits::eTransferDst)
                      .or_panic();

      temp.write(staging_buff);
      ssbuffers_.emplace_back(std::move(temp));
    }

    // == Uniform Buffers ==============================================================================================
    uniform_buffers_.clear();
    uniform_buffers_mapped_.clear();
    {
      vk::DeviceSize size_bytes = sizeof(UniformBufferObject);
      for (auto i = 0; i < kMaxFramesInFlight; ++i) {
        auto ubo = vkren::BufferResource::create_persistently_mapped_uniform_buffer(device_, size_bytes).or_panic();

        // This technique is called persistent mapping, the buffer stays mapped for the application's whole life-time.
        // It increases performance as the mapping process is not free.
        uniform_buffers_mapped_.emplace_back(ubo.mapped_data);
        uniform_buffers_.emplace_back(std::move(ubo.buffer));
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

  /**
   * @brief Writes the commands we what to execute into a command buffer
   *
   * @param image_index
   */
  void record_graphics_command_buffer(size_t frame_index, uint32_t image_index) {
    swap_chain_.begin_rendering(graphics_command_buffers_[frame_index], image_index);

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
    graphics_command_buffers_[frame_index].bindVertexBuffers(0, {ssbuffers_[current_frame_].vk_buffer()}, {0});
    graphics_command_buffers_[frame_index].draw(ParticleSystem::kParticleCount, 1, 0, 0);

    swap_chain_.end_rendering(graphics_command_buffers_[frame_index], image_index);
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

  void create_descriptors() {
    dsl_manager_   = vkren::DescriptorSetLayoutManager::create(device_);
    auto ratios    = vkren::DescriptorPoolSizeRatio::create_default();
    dsl_allocator_ = vkren::DescriptorAllocator::create_and_init(device_, 100, ratios).or_panic();
    auto result    = vkren::DescriptorSetBuilder::create(dsl_manager_, dsl_allocator_)
                      .with_binding(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute)
                      .with_binding(vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
                      .with_binding(vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
                      .build_many(kMaxFramesInFlight)
                      .or_panic("Could not create descriptor sets");

    compute_descriptor_sets_       = std::move(result.descriptor_sets);
    compute_descriptor_set_layout_ = result.layout;

    auto writer = vkren::DescriptorSetWriter::create(device_);
    for (auto i = 0U; i < kMaxFramesInFlight; ++i) {
      auto last_ind = (i - 1) % kMaxFramesInFlight;
      auto curr_ind = i;
      writer.write_buffer(0, uniform_buffers_[i].desc_buffer_info(), vk::DescriptorType::eUniformBuffer);
      writer.write_buffer(1, ssbuffers_[last_ind].desc_buffer_info(), vk::DescriptorType::eStorageBuffer);
      writer.write_buffer(2, ssbuffers_[curr_ind].desc_buffer_info(), vk::DescriptorType::eStorageBuffer);
      writer.write_to_set(compute_descriptor_sets_[i]);
    }
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
  vk::DescriptorSetLayout compute_descriptor_set_layout_ = nullptr;

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

  std::vector<eray::vkren::BufferResource> uniform_buffers_;
  std::vector<void*> uniform_buffers_mapped_;

  vk::raii::DescriptorPool descriptor_pool_ = nullptr;
  std::vector<vk::DescriptorSet> compute_descriptor_sets_;

  vk::raii::ImageView txt_view_  = nullptr;
  vk::raii::Sampler txt_sampler_ = nullptr;

  std::vector<vkren::BufferResource> ssbuffers_;

  std::shared_ptr<eray::os::Window> window_;

  vkren::DescriptorSetLayoutManager dsl_manager_ = vkren::DescriptorSetLayoutManager(nullptr);
  vkren::DescriptorAllocator dsl_allocator_      = vkren::DescriptorAllocator(nullptr);

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
  using Logger = eray::util::Logger;
  using System = eray::os::System;

  Logger::instance().init();
  Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());

  auto window_creator =
      eray::os::VulkanGLFWWindowCreator::create().or_panic("Could not create a Vulkan GLFW window creator");
  System::init(std::move(window_creator)).or_panic("Could not initialize Operating System API");

  auto app = ComputeParticlesApplication();
  app.run();

  eray::os::System::instance().terminate();

  return 0;
}
