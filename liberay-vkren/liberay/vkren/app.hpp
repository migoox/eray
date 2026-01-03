#pragma once

#include <imgui/imgui.h>

#include <chrono>
#include <liberay/os/file_dialog.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/vkren/deletion_queue.hpp>
#include <liberay/vkren/descriptor.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/render_graph.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

using namespace std::chrono_literals;

struct VulkanApplicationContext {
  /**
   * @brief Responsible for dynamic loading of the Vulkan library, it's a starting point for creating other RAII-based
   * Vulkan objects like vk::raii::Instance or vk::raii::Device. e.g. context_.createInstance(...) wraps C-style
   * vkCreateInstance.
   *
   */
  vk::raii::Context vk_context;

  /**
   * @brief Vulkan logical device wrapper. Provides all functionalities of Vulkan and additional high-level features
   * implemented in vkren.
   */
  std::unique_ptr<Device> device = nullptr;

  /**
   * @brief Vulkan swap chain wrapper. Maintains the swap chain queue and provides basic attachments. ImGui is rendered
   * the image attachment of the swap chain.
   */
  std::unique_ptr<SwapChain> swap_chain = nullptr;

  DescriptorSetLayoutManager dsl_manager = DescriptorSetLayoutManager(nullptr);
  DescriptorAllocator dsl_allocator      = DescriptorAllocator(nullptr);

  /**
   * @brief Represents rendering API agnostic window that the Vulkan application is rendered to.
   */
  std::shared_ptr<eray::os::Window> window = nullptr;

  /**
   * @brief Main render graph used by the application.
   */
  RenderGraph render_graph;
};

struct VulkanApplicationCreateInfo {
  std::string app_name = "Application";

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
};

class VulkanApplication {
 public:
  using Duration = std::chrono::nanoseconds;
  using Clock    = std::chrono::high_resolution_clock;

  explicit VulkanApplication(std::nullptr_t) {}

  VulkanApplication(const VulkanApplication&)                = delete;
  VulkanApplication(VulkanApplication&&) noexcept            = default;
  VulkanApplication& operator=(const VulkanApplication&)     = delete;
  VulkanApplication& operator=(VulkanApplication&&) noexcept = default;

  template <typename TDerived>
  static TDerived create(VulkanApplicationCreateInfo&& create_info = {}) {
    auto app         = TDerived();
    app.create_info_ = std::move(create_info);
    return app;
  }

  virtual ~VulkanApplication() = default;

  void run();

  // Multiple frames are created in flight at once. Rendering of one frame does not interfere with the recording of
  // the other. We choose the number 2, because we don't want the CPU to go to far ahead of the GPU.
  static constexpr int kMaxFramesInFlight = 2;

 protected:
  /**
   * @brief Allows for custom Vulkan Device creation. Useful when you want to set non default profiles.
   *
   */
  virtual std::unique_ptr<Device> create_device();

  /**
   * @brief If not provided and the `enable_msaa` is set to `true`,
   * maximum usable sample count will be used.
   */
  virtual vk::SampleCountFlagBits get_msaa_sample_count(const vk::raii::PhysicalDevice& physical_device);

  /**
   * @brief Invoked when clear color value is needed.
   */
  virtual vk::ClearColorValue get_clear_color_value() {
    return vk::ClearColorValue(std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F});
  }

  /**
   * @brief Invoked when clear depth stencil value is needed.
   */
  virtual vk::ClearDepthStencilValue get_clear_depth_stencil_value() {
    return vk::ClearDepthStencilValue{.depth = 1.0F, .stencil = 0};
  }

  /**
   * @brief Called right after the window is initialized.
   */
  virtual void on_window_setup(os::Window& /*window*/) {}

  /**
   * @brief Called after the window and Vulkan initialization and before the main loop.
   */
  virtual void on_init() {}

  /**
   * brief Called right after all the window events are polled.
   */
  virtual void on_input_events_polled() {}

  /**
   * brief Called right after all the window events are polled.
   */
  virtual void on_input_events_polled(bool /*gui_captured*/) {}

  /**
   * brief Called right after all the window events are polled.
   */
  virtual void on_input_events_polled(bool /*gui_mouse_captured*/, bool /*gui_keyboard_captured*/) {}

  /**
   * @brief Called on each physics update (synchronously) with fixed time step. To change time step use `set_tick_time`.
   */
  virtual void on_process_physics_generic(Duration /*delta*/) {}

  /**
   * @brief Called on each physics update (synchronously) with fixed time step. To change time step use `set_tick_time`.
   */
  virtual void on_process_physics(float /*delta*/) {}

  /**
   * @brief Called after `on_imgui()` finishes before the rendering begins.
   *
   */
  virtual void on_process_generic(Duration /*delta*/) {}

  /**
   * @brief Called after `on_imgui()` finishes before the rendering begins.
   *
   */
  virtual void on_process(float /*delta*/) {}

  /**
   * @brief Designed to update dynamic GPU resources, e.g. UBOs that are updated per frames. The method execution
   * happens simultaneously with previous frame GPU rendering, which means that it requires to create resource per frame
   * in flight.
   */
  virtual void on_frame_prepare(uint32_t /*current_frame*/, Duration /*delta*/) {}

  /**
   * @brief Designed to update dynamic GPU resources, e.g. UBOs that are updated per frames. The GPU execution never
   * overlaps with this method execution, so there is no need to create a resource per frame in flight. This function
   * is called only if frame data is marked dirty, see `mark_frame_data_dirty()`.
   *
   * @warning Avoid doing heavy operations in this method as it stalls both CPU and GPU. This
   * method should be responsible only for uploading the data. If you need to perform some data calculations use
   * `on_update()` instead.
   */
  virtual void on_frame_prepare_sync(Duration /*delta*/) {}

  /**
   * @brief Called right after ImGui is prepared for drawing a new frame.
   */
  virtual void on_imgui() { ImGui::ShowDemoWindow(); }

  /**
   * @brief Called right after ImGui is prepared for drawing a new frame.
   */
  virtual void on_imgui(float /*delta*/) { ImGui::ShowDemoWindow(); }

  /**
   * @brief Invoked when the graphics command buffer gets recorded.
   */
  virtual void on_record_graphics(vk::raii::CommandBuffer& /*graphics_command_buffer*/, uint32_t /*current_frame*/) {}

  /**
   * @brief Called after the main loop exits, before destruction.
   */
  virtual void on_destroy() {}

  void mark_frame_data_dirty() { frame_data_dirty_ = true; }

  /**
   * @brief Returns current frames per seconds.
   */
  std::uint16_t fps() const { return fps_; }

  /**
   * @brief Returns current physics ticks per seconds.
   */
  std::uint16_t tps() const { return tps_; }

  /**
   * @brief Returns time in seconds from start of the app.
   *
   * @warning Casting this value to float directly might produce numerical instability the older the application is.
   */
  Duration time() const { return time_; }
  void set_tick_time(Duration tick_time) { tick_time_ = tick_time; }

  VulkanApplicationContext& ctx() { return context_; }
  const VulkanApplicationContext& ctx() const { return context_; }

  os::FileDialog& file_dialog() { return os::System::file_dialog(); }

  VulkanApplication() = default;

 private:
  void init_vk();
  void init_imgui();

  void main_loop();
  void render_frame(Duration delta);

  /**
   * @brief Writes the commands we what to execute into a command buffer
   *
   * @param image_index
   */
  void record_graphics_command_buffer(size_t frame_index, uint32_t image_index);

  void destroy();

  void create_dsl();
  void create_swap_chain();
  void create_command_pool();
  void create_command_buffers();
  void create_sync_objs();

 private:
  VulkanApplicationContext context_;

  static constexpr Duration kDefaultTickTime = 16666us;  // 60 TPS = 16.6(6) ms/t

  Duration tick_time_ = kDefaultTickTime;
  Duration time_      = 0ns;
  std::uint16_t fps_  = 0;
  std::uint16_t tps_  = 0;
  Duration lag_       = 0ns;
  Duration second_    = 0ns;
  uint16_t frames_    = 0U;
  uint16_t ticks_     = 0U;

  /**
   * @brief Command pools manage the memory that is used to store the buffers and command buffers are allocated from
   * them.
   *
   */
  vk::raii::CommandPool command_pool_ = nullptr;

  uint32_t current_semaphore_ = 0;
  uint32_t current_frame_     = 0;

  /**
   * @brief Drawing operations are recorded in command buffer objects.
   *
   */
  std::array<vk::raii::CommandBuffer, kMaxFramesInFlight> graphics_command_buffers_ = {nullptr, nullptr};

  /**
   * @brief Semaphores are used to assert on GPU that a process e.g. rendering is finished.
   *
   */
  std::vector<vk::raii::Semaphore> acquire_image_semaphores_;
  std::vector<vk::raii::Semaphore> render_finished_semaphores_;

  std::vector<vk::PipelineStageFlags> submit_stage_masks_;

  /**
   * @brief Fences are used to block GPU until the frame is presented.
   *
   */
  std::array<vk::raii::Fence, kMaxFramesInFlight> record_fences_ = {nullptr, nullptr};

  vk::DescriptorSetLayout dsl_;
  vk::raii::DescriptorPool imgui_descriptor_pool_ = nullptr;

  VulkanApplicationCreateInfo create_info_;

  DeletionQueue deletion_queue_;

  bool frame_data_dirty_ = true;
};

}  // namespace eray::vkren
