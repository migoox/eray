#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <exception>
#include <liberay/os/system.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/vkren/app.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace eray::vkren {

void VulkanApplication::run() {
  context_.window = eray::os::System::instance().create_window().or_panic("Could not create a window");
  context_.window->set_title(create_info_.app_name);
  on_window_setup(*context_.window);
  context_.physics_input_manager = os::InputManager::create(context_.window);
  context_.frame_input_manager   = os::InputManager::create(context_.window);
  current_input_manager_         = context_.frame_input_manager.get();

  init_vk();
  init_imgui();
  on_init();
  main_loop();
  destroy();
}

std::unique_ptr<Device> VulkanApplication::create_device() {
  auto desktop_profile                  = Device::CreateInfo::DesktopProfile{};
  auto device_info                      = desktop_profile.get(*context_.window);
  device_info.app_info.pApplicationName = create_info_.app_name.c_str();
  return Device::create(context_.vk_context, device_info).or_panic("Could not create a logical device wrapper");
}

void VulkanApplication::init_vk() {
  context_.device = create_device();
  create_swap_chain();
  create_command_pool();
  create_command_buffers();
  create_sync_objs();
}

void VulkanApplication::main_loop() {
  auto& imgui_io     = ImGui::GetIO();
  auto previous_time = Clock::now();
  while (!context_.window->should_close()) {
    auto current_time = Clock::now();
    auto delta        = current_time - previous_time;
    previous_time     = current_time;
    lag_ += std::chrono::duration_cast<Duration>(delta);
    second_ += std::chrono::duration_cast<Duration>(delta);

    // == Process Window events ========================================================================================
    context_.window->poll_events();
    context_.window->process_queued_events();

    // == Fixed time step update =======================================================================================
    auto tick_time_flt = std::chrono::duration<float>(delta).count();

    current_input_manager_ = context_.physics_input_manager.get();
    while (lag_ >= tick_time_) {
      context_.physics_input_manager->prepare(imgui_io.WantCaptureMouse || imgui_io.WantCaptureKeyboard);
      on_process_physics(tick_time_flt);
      on_process_physics_generic(tick_time_);
      context_.physics_input_manager->process();

      lag_ -= tick_time_;
      time_ += tick_time_;
      ++ticks_;
    }
    current_input_manager_ = context_.frame_input_manager.get();

    // == File dialog update ===========================================================================================
    if (auto result = os::System::file_dialog().update(); !result) {
      util::Logger::err("File dialog update failed");
    }

    // == Render =======================================================================================================
    auto delta_flt = std::chrono::duration<float>(delta).count();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();
    on_imgui(delta_flt);
    on_imgui();
    ImGui::Render();

    context_.frame_input_manager->prepare(imgui_io.WantCaptureMouse || imgui_io.WantCaptureKeyboard);
    on_process(delta_flt);
    on_process_generic(delta);
    context_.frame_input_manager->process();

    render_frame(delta);
    frames_++;

    if (imgui_io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      ImGui::UpdatePlatformWindows();
      ImGui::RenderPlatformWindowsDefault();
    }

    // == Update FPS and TPS ===========================================================================================
    if (second_ > 1s) {
      uint16_t seconds = static_cast<uint16_t>(std::chrono::duration_cast<std::chrono::seconds>(second_).count());

      fps_    = static_cast<uint16_t>(frames_ / seconds);
      tps_    = static_cast<uint16_t>(ticks_ / seconds);
      frames_ = 0;
      ticks_  = 0;
      second_ = 0ns;
    }
  }

  // Since draw frame operations are async, when the main loop ends the drawing operations may still be going on.
  // This call is allows for the async operations to finish before cleaning the resources.
  context_.device->vk().waitIdle();
}

void VulkanApplication::render_frame(Duration delta) {
  // If rendering for the current frame has not finished yet, CPU waits for the GPU
  while (vk::Result::eTimeout ==
         context_.device->vk().waitForFences(*record_fences_[current_frame_], vk::True, UINT64_MAX)) {
    ;
  }
  context_.device->vk().resetFences(*record_fences_[current_frame_]);
  graphics_command_buffers_[current_frame_].reset();

  // Get the image from the swap chain. When the image is ready ready the present semaphore will be signaled.
  uint32_t image_index{};
  if (auto acquire_opt = context_.swap_chain->acquire_next_image(
          UINT64_MAX, *acquire_image_semaphores_[current_semaphore_], nullptr)) {
    if (acquire_opt->status != SwapChain::AcquireResult::Status::Success) {
      return;
    }
    image_index = acquire_opt->image_index;
  } else {
    eray::util::panic("Failed to acquire next image!");
    return;
  }
  record_graphics_command_buffer(current_frame_, image_index);
  on_frame_prepare(current_frame_, delta);

  auto wait_dst_stage_mask = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eColorAttachmentOutput);
  auto submit_info         = vk::SubmitInfo{
              .waitSemaphoreCount   = 1,
              .pWaitSemaphores      = &*acquire_image_semaphores_[current_semaphore_],
              .pWaitDstStageMask    = &wait_dst_stage_mask,
              .commandBufferCount   = 1,
              .pCommandBuffers      = &*graphics_command_buffers_[current_frame_],
              .signalSemaphoreCount = 1,
              .pSignalSemaphores    = &*render_finished_semaphores_[image_index],
  };

  if (frame_data_dirty_) {
    auto prev_frame = (kMaxFramesInFlight + current_frame_ - 1) % kMaxFramesInFlight;
    while (vk::Result::eTimeout ==
           context_.device->vk().waitForFences(*record_fences_[prev_frame], vk::True, UINT64_MAX)) {
      ;
    }
    on_frame_prepare_sync(delta);
    frame_data_dirty_ = false;
  }
  context_.device->graphics_queue().submit(submit_info, *record_fences_[current_frame_]);

  // The image will not be presented until the render finished semaphore is signaled by the submit call.
  const auto present_info = vk::PresentInfoKHR{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores    = &*render_finished_semaphores_[image_index],
      .swapchainCount     = 1,
      .pSwapchains        = &***context_.swap_chain,
      .pImageIndices      = &image_index,
      .pResults           = nullptr,
  };

  if (!context_.swap_chain->present_image(present_info)) {
    eray::util::Logger::err("Failed to present an image!");
  }

  current_semaphore_ = (current_semaphore_ + 1) % acquire_image_semaphores_.size();
  current_frame_     = (current_frame_ + 1) % kMaxFramesInFlight;
}

void VulkanApplication::destroy() {
  on_destroy();
  deletion_queue_.flush();
  context_.swap_chain->destroy();

  eray::util::Logger::succ("Successfully destroyed the vulkan application");
}

vk::SampleCountFlagBits VulkanApplication::get_msaa_sample_count(const vk::raii::PhysicalDevice& /*physical_device*/) {
  return context_.device->max_usable_sample_count();
}

void VulkanApplication::create_swap_chain() {
  context_.swap_chain = SwapChain::create(*context_.device, context_.window,
                                          get_msaa_sample_count(context_.device->physical_device()), create_info_.vsync)
                            .or_panic("Could not create a swap chain");
}

void VulkanApplication::create_command_pool() {
  auto command_pool_info = vk::CommandPoolCreateInfo{
      // There are two possible flags for command pools:
      // - VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are rerecorded with new commands very often
      //   (may change memory allocation behavior).
      // - VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be rerecorded individually,
      //   without this flag they all have to be reset together. (Reset and rerecord over it in every frame)

      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,

      // Each command pool can only allocate command buffers that are submitted on a single type of queue.
      // We setup commands for drawing, and thus we've chosen the graphics queue family.
      .queueFamilyIndex = context_.device->graphics_queue_family(),
  };

  command_pool_ =
      Result(context_.device->vk().createCommandPool(command_pool_info)).or_panic("Could not create a command pool");
}

void VulkanApplication::create_command_buffers() {
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
      Result(context_.device->vk().allocateCommandBuffers(alloc_info)).or_panic("Could not allocate a command buffer");
  std::ranges::move(result | std::views::take(kMaxFramesInFlight), graphics_command_buffers_.begin());
}

void VulkanApplication::create_sync_objs() {
  acquire_image_semaphores_.clear();
  render_finished_semaphores_.clear();

  for (size_t i = 0; i < context_.swap_chain->images().size(); ++i) {
    if (auto result = context_.device->vk().createSemaphore(vk::SemaphoreCreateInfo{})) {
      acquire_image_semaphores_.emplace_back(std::move(*result));
    } else {
      eray::util::panic("Could not create a semaphore");
    }
  }

  for (size_t i = 0; i < context_.swap_chain->images().size(); ++i) {
    if (auto result = context_.device->vk().createSemaphore(vk::SemaphoreCreateInfo{})) {
      render_finished_semaphores_.emplace_back(std::move(*result));
    } else {
      eray::util::panic("Could not create a semaphore");
    }
  }

  for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
    if (auto result =
            context_.device->vk().createFence(vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled})) {
      record_fences_[i] = std::move(*result);
    } else {
      eray::util::panic("Could not create a fence");
    }
  }
}

void VulkanApplication::record_graphics_command_buffer(size_t frame_index, uint32_t image_index) {
  auto clear_color_value         = get_clear_color_value();
  auto clear_depth_stencil_value = get_clear_depth_stencil_value();

  graphics_command_buffers_[frame_index].begin({});
  {
    auto cmd_buff = vk::CommandBuffer{graphics_command_buffers_[frame_index]};
    context_.render_graph.emit(*context_.device, cmd_buff);
  }
  context_.swap_chain->begin_rendering(graphics_command_buffers_[frame_index], image_index, clear_color_value,
                                       clear_depth_stencil_value);

  // Scissor rectangle defines in which region pixels will actually be stored. The rasterizer will discard any
  // pixels outside the scissored rectangle. We want to draw to entire framebuffer.
  graphics_command_buffers_[frame_index].setScissor(
      0, vk::Rect2D{.offset = vk::Offset2D{.x = 0, .y = 0}, .extent = context_.swap_chain->extent()});
  graphics_command_buffers_[frame_index].setViewport(
      0, vk::Viewport{
             .x      = 0.0F,
             .y      = 0.0F,
             .width  = static_cast<float>(context_.swap_chain->extent().width),
             .height = static_cast<float>(context_.swap_chain->extent().height),
             // Note: min and max depth must be between [0.0F, 1.0F] and min might be higher than max.
             .minDepth = 0.0F,
             .maxDepth = 1.0F  //
         });
  on_record_graphics(graphics_command_buffers_[frame_index], static_cast<uint32_t>(frame_index));

  {
    vk::CommandBuffer cmd_buff = graphics_command_buffers_[frame_index];
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd_buff));
  }

  context_.swap_chain->end_rendering(graphics_command_buffers_[frame_index], image_index);
  graphics_command_buffers_[frame_index].end();
}

static void check_vk_result(VkResult err) {
  if (err == 0) {
    return;
  }
  util::Logger::info("ImGui Vulkan Error: VkResult = {}", vk::to_string(vk::Result(err)));
  if (err < 0) {
    std::terminate();
  }
}

void VulkanApplication::init_imgui() {
  // 1: create descriptor pool for IMGUI
  //  the size of the pool is very oversize, but it's copied from imgui demo itself.
  vk::DescriptorPoolSize pool_sizes[] = {{.type = vk::DescriptorType::eSampler, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eSampledImage, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eStorageImage, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eUniformTexelBuffer, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eStorageTexelBuffer, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eUniformBufferDynamic, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eStorageBufferDynamic, .descriptorCount = 1000},
                                         {.type = vk::DescriptorType::eInputAttachment, .descriptorCount = 1000}};

  vk::DescriptorPoolCreateInfo pool_info = {
      .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets       = 1000,
      .poolSizeCount = std::size(pool_sizes),
      .pPoolSizes    = pool_sizes,
  };

  imgui_descriptor_pool_ = Result(context_.device->vk().createDescriptorPool(pool_info))
                               .or_panic("Could not create a descriptor pool for ImGui");

  // 2: initialize imgui library

  // this initializes the core structures of imgui
  ImGui::CreateContext();

  // this initializes imgui for SDL
  if (context_.window->window_api() != os::WindowAPI::GLFW) {
    util::panic("Could not initialize imgui context: only GLFW is supported");
  }
  ImGui_ImplGlfw_InitForVulkan(reinterpret_cast<GLFWwindow*>(context_.window->win_ptr()), true);

  // this initializes imgui for Vulkan
  vk::Instance instance                    = context_.device->instance();
  vk::PhysicalDevice physical_device       = context_.device->physical_device();
  vk::Device device                        = **context_.device;
  vk::Queue graphics_queue                 = context_.device->graphics_queue();
  vk::DescriptorPool imgui_descriptor_pool = imgui_descriptor_pool_;

  auto color_format         = static_cast<VkFormat>(context_.swap_chain->color_attachment_format());
  auto depth_stencil_format = static_cast<VkFormat>(context_.swap_chain->depth_stencil_attachment_format());

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.Instance                    = static_cast<VkInstance>(instance);
  init_info.PhysicalDevice              = static_cast<VkPhysicalDevice>(physical_device);
  init_info.Device                      = static_cast<VkDevice>(device);
  init_info.QueueFamily                 = context_.device->graphics_queue_family();
  init_info.Queue                       = static_cast<VkQueue>(graphics_queue);
  init_info.DescriptorPool              = static_cast<VkDescriptorPool>(imgui_descriptor_pool);
  init_info.RenderPass                  = VK_NULL_HANDLE;
  init_info.MinImageCount               = context_.swap_chain->min_image_count();
  init_info.ImageCount                  = static_cast<uint32_t>(context_.swap_chain->images().size());
  init_info.MSAASamples                 = static_cast<VkSampleCountFlagBits>(context_.swap_chain->msaa_sample_count());
  init_info.PipelineCache               = VK_NULL_HANDLE;
  init_info.Subpass                     = 0;
  init_info.UseDynamicRendering         = true;
  init_info.PipelineRenderingCreateInfo = {
      .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .pNext                   = nullptr,
      .viewMask                = 0,
      .colorAttachmentCount    = 1,
      .pColorAttachmentFormats = &color_format,
      .depthAttachmentFormat   = depth_stencil_format,
      .stencilAttachmentFormat = depth_stencil_format,
  };
  init_info.Allocator         = nullptr;
  init_info.CheckVkResultFn   = check_vk_result;
  init_info.MinAllocationSize = 1024 * 1024;

  ImGui_ImplVulkan_Init(&init_info);
  ImGui_ImplVulkan_CreateFontsTexture();

  deletion_queue_.push_deletor([]() { ImGui_ImplVulkan_Shutdown(); });

  util::Logger::succ("Successfully initialized ImGui");
}

}  // namespace eray::vkren
