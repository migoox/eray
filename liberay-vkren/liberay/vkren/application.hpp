#pragma once

#include <imgui/imgui.h>

#include <functional>
#include <liberay/os/window/window.hpp>
#include <liberay/vkren/deletion_queue.hpp>
#include <liberay/vkren/descriptor.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <optional>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

struct VulkanApplicationContext {
  /**
   * @brief Responsible for dynamic loading of the Vulkan library, it's a starting point for creating other RAII-based
   * Vulkan objects like vk::raii::Instance or vk::raii::Device. e.g. context_.createInstance(...) wraps C-style
   * vkCreateInstance.
   *
   */
  vk::raii::Context vk_context_;

  Device device_                            = Device(nullptr);
  SwapChain swap_chain_                     = SwapChain(nullptr);
  DescriptorSetLayoutManager dsl_manager_   = DescriptorSetLayoutManager(nullptr);
  DescriptorAllocator dsl_allocator_        = DescriptorAllocator(nullptr);
  std::shared_ptr<eray::os::Window> window_ = nullptr;

  static constexpr int kMaxFramesInFlight = 2;
};

struct VulkanApplicationCreateInfo {
  std::string app_name = "Application";

  /**
   * @brief Allows for custom Vulkan Device creation. Useful when you want to set non default profiles.
   *
   */
  std::optional<std::function<Device()>> device_creator = std::nullopt;

  /**
   * @brief Enables MSAA.
   *
   */
  bool enable_msaa = true;

  /**
   * @brief Enables VSync.
   *
   */
  bool vsync = true;

  /**
   * @brief If not provided and the `enable_msaa` is set to `true`, maximum usable sample count will be used.
   *
   */
  std::optional<std::function<vk::SampleCountFlagBits(const vk::raii::PhysicalDevice& physical_device)>>
      msaa_sample_count_getter = std::nullopt;

  /**
   * @brief Invoked when clear color value is needed.
   *
   */
  std::optional<std::function<vk::ClearColorValue()>> clear_color_value_getter = std::nullopt;

  /**
   * @brief Invoked when clear depth stencil value is needed.
   *
   */
  std::optional<std::function<vk::ClearDepthStencilValue()>> clear_depth_stencil_getter = std::nullopt;

  /**
   * @brief Called after the window and Vulkan initialization and before the main application loop.
   *
   */
  std::function<void(VulkanApplicationContext& ctx)> on_init = [](VulkanApplicationContext&) {};

  /**
   * @brief Called right after the window is initialized.
   *
   */
  std::function<void(os::Window& window)> on_window_setup = [](os::Window&) {};

  /**
   * @brief Called after the new image is acquired, before the graphics command buffer is recorded. Useful for UBO
   * updating.
   *
   */
  std::function<void(VulkanApplicationContext& ctx, uint32_t image_index)> on_frame_prepare =
      [](VulkanApplicationContext&, uint32_t) {};

  /**
   * @brief Called right after the imgui is prepared for drawing a new frame.
   *
   */
  std::function<void(VulkanApplicationContext& ctx)> on_imgui = [](VulkanApplicationContext&) {
    ImGui::ShowDemoWindow();
  };

  /**
   * @brief Invoked when graphics command buffer gets recorded.
   *
   */
  std::function<void(VulkanApplicationContext& ctx, vk::raii::CommandBuffer& graphics_command_buffer,
                     uint32_t image_index)>
      on_record_graphics = [](VulkanApplicationContext&, vk::raii::CommandBuffer&, uint32_t) {};

  /**
   * @brief Called after the main loop is exited before anything has been destroyed.
   *
   */
  std::function<void()> on_destroy = []() {};
};

class VulkanApplication {
 public:
  explicit VulkanApplication(std::nullptr_t) {}
  static VulkanApplication create(VulkanApplicationCreateInfo&& create_info);

  void run();

 private:
  VulkanApplication() = default;

  void init_vk();
  void init_imgui();

  void main_loop();
  void draw_frame();

  /**
   * @brief Writes the commands we what to execute into a command buffer
   *
   * @param image_index
   */
  void record_graphics_command_buffer(size_t frame_index, uint32_t image_index);

  void destroy();

  void create_dsl();
  void create_device();
  void create_swap_chain();
  void create_command_pool();
  void create_command_buffers();
  void create_sync_objs();

  // Multiple frames are created in flight at once. Rendering of one frame does not interfere with the recording of
  // the other. We choose the number 2, because we don't want the CPU to go to far ahead of the GPU.
  static constexpr int kMaxFramesInFlight = VulkanApplicationContext::kMaxFramesInFlight;

 private:
  VulkanApplicationContext context_;

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

  vk::DescriptorSetLayout dsl_;
  vk::raii::DescriptorPool imgui_descriptor_pool_ = nullptr;

  VulkanApplicationCreateInfo create_info_;

  DeletionQueue deletion_queue_;
};

}  // namespace eray::vkren
