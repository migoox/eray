#pragma once

#include <functional>
#include <liberay/os/window/window.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/image_description.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_profiles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

/**
 * @brief Simplifies logical device creation and provides additional functions not available in `vk::raii:Device`.
 * One can access the vk::raii::Device via -> and * operators.
 *
 */
class Device {
 public:
  /**
   * @brief Creates uninitialized empty Device. Useful for postponed creation, usually when the Device is a class
   * member.
   *
   * @warning This constructor is unsafe. It's programmer responsibility to overwrite the empty device with proper
   * initialized one.
   *
   */
  explicit Device(std::nullptr_t) {}

  ERAY_DELETE_COPY(Device)
  ERAY_DEFAULT_MOVE(Device)

  using SurfaceCreator = std::function<std::optional<vk::raii::SurfaceKHR>(vk::raii::Instance&)>;

  struct CreateInfo {
    /**
     * @brief Allow to request a profile properties.
     *
     */
    VpProfileProperties profile_properties = {};

    /**
     * @brief When empty, validation layers are completely disabled, otherwise "VK_EXT_debug_utils" global extension
     * will be automatically requested (no matter if specified in global_extensions explicitly).
     *
     */
    std::span<const char* const> validation_layers;

    /**
     * @note If validation layers are not empty, "VK_EXT_debug_utils" is inserted automatically.
     *
     */
    std::span<const char* const> global_extensions;

    std::span<const char* const> device_extensions;

    /**
     * @brief Lambda that creates a surface, e.g. when using GLFW the creator lambda would call glfwCreateWindowSurface.
     * If the surface creation fails the lambda should return the SurfaceCreationError.
     *
     */
    SurfaceCreator surface_creator;

    /**
     * @brief Severity flags used for debug messenger.
     *
     * @note If validation layers are empty, no debug messenger will be set up.
     *
     */
    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags;

    vk::ApplicationInfo app_info;

    /**
     * @brief `DesktopTemplate` provides default device configuration for desktop platforms.
     *
     */
    struct DesktopProfile {
      /**
       * @brief Returns `Device::CreateInfo` basing dedicated for generic desktop platforms. Adds
       * `"VK_LAYER_KHRONOS_validation"` in debug mode.
       *
       * @param surface_creator creates a SurfaceKHR, e.g. when using GLFW, this lambda would call
       * `glfwCreateWindowSurface`
       * @param required_instance_extensions allows to provide instance extensions that are needed by platform, e.g.
       * when using GLFW you can acquire these instance extensions with `glfwGetRequiredInstanceExtensions`
       *
       * @warning `DesktopTemplate` must outlive the `CreateInfo` because it is an owner of the allocated data.
       *
       * @return CreateInfo
       */
      CreateInfo get(const SurfaceCreator& surface_creator_func,
                     std::span<const char* const> required_global_extensions) noexcept;

      /**
       * @brief Defines proper settings basing on the window api it's handle.
       *
       * @param window
       * @return CreateInfo
       */
      CreateInfo get(const eray::os::Window& window) noexcept;

     private:
      std::vector<const char*> validation_layers_;
      std::vector<const char*> global_extensions_;
      std::vector<const char*> device_extensions_;

      SurfaceCreator surface_creator_;
    };
  };

  static Result<Device, Error> create(vk::raii::Context& ctx, const CreateInfo& info) noexcept;

  vk::raii::Instance& instance() noexcept { return instance_; }
  const vk::raii::Instance& instance() const noexcept { return instance_; }

  vk::raii::PhysicalDevice& physical_device() noexcept { return physical_device_; }
  const vk::raii::PhysicalDevice& physical_device() const noexcept { return physical_device_; }

  vk::raii::Device* operator->() noexcept { return &device_; }
  const vk::raii::Device* operator->() const noexcept { return &device_; }

  vk::raii::Device& operator*() noexcept { return device_; }
  const vk::raii::Device& operator*() const noexcept { return device_; }

  vk::raii::SurfaceKHR& surface() noexcept { return surface_; }
  const vk::raii::SurfaceKHR& surface() const noexcept { return surface_; }

  uint32_t graphics_queue_family() const { return graphics_queue_family_; }
  vk::raii::Queue& graphics_queue() noexcept { return graphics_queue_; }
  const vk::raii::Queue& graphics_queue() const noexcept { return graphics_queue_; }

  uint32_t compute_queue_family() const { return compute_queue_family_; }
  vk::raii::Queue& compute_queue() noexcept { return compute_queue_; }
  const vk::raii::Queue& compute_queue() const noexcept { return compute_queue_; }

  uint32_t presentation_queue_family() const { return presentation_queue_family_; }
  vk::raii::Queue& presentation_queue() noexcept { return presentation_queue_; }
  const vk::raii::Queue& presentation_queue() const noexcept { return presentation_queue_; }

  Result<uint32_t, Error> find_mem_type(uint32_t type_filter, vk::MemoryPropertyFlags props) const;

  template <typename T>
  using OptRef = std::optional<std::reference_wrapper<T>>;

  /**
   * @brief To end the single_time commands, see `end_single_time_commands`.
   *
   * @param command_pool
   * @return vk::raii::CommandBuffer
   */
  [[nodiscard]] vk::raii::CommandBuffer begin_single_time_commands(
      OptRef<vk::raii::CommandPool> command_pool = std::nullopt) const;

  /**
   * @brief Blocks the CPU until the commands are submitted.
   *
   * @param cmd_buff
   */
  void end_single_time_commands(vk::raii::CommandBuffer& cmd_buff) const;

  /**
   * @brief Non-blocking, defines a pipeline barrier.
   *
   * @param image
   * @param old_layout
   * @param new_layout
   */
  void transition_image_layout(const vk::raii::Image& image, const ImageDescription& image_desc,
                               vk::ImageLayout old_layout, vk::ImageLayout new_layout) const;

  Result<void, Error> generate_mipmaps(vk::raii::Image& image, const ImageDescription& image_desc) const;

  vk::SampleCountFlagBits max_usable_sample_count() const;

 private:
  Device() = default;

  Result<void, Error> create_instance(vk::raii::Context& ctx, const CreateInfo& info) noexcept;
  Result<void, Error> create_debug_messenger(const CreateInfo& info) noexcept;
  Result<void, Error> pick_physical_device(const CreateInfo& info) noexcept;
  Result<void, Error> create_logical_device(const CreateInfo& info) noexcept;
  Result<void, Error> create_command_pool() noexcept;

  std::vector<const char*> global_extensions(const CreateInfo& info) noexcept;

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                         vk::DebugUtilsMessageTypeFlagsEXT type,
                                                         const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                         void*);

  static auto default_feature_chain() {
    return vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                              vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>{
        {},
        {.synchronization2 = vk::True, .dynamicRendering = vk::True},  // Enable dynamic rendering from Vulkan 1.3
        {.extendedDynamicState = vk::True}  // Enable extended dynamic state from the extension
    };
  }

 private:
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
   * @brief Vulkan allows for off-screen rendering, as well as rendering to a surface that is being displayed in
   * any Windowing API. This concept applies to mobile too. The SurfaceKHR usage is platform-agnostic, however it's
   * creation is not.
   * - On Linux with Wayland you need "VK_KHR_wayland_surface" and on windows you need
   * "VK_KHR_win32_surface" instance extension. Luckily the GLFW's `glfwGetRequiredInstanceExtensions` properly
   * returns the platform specific Vulkan extensions. For this reason SurfaceCreator is mandatory.
   * - Each extension provides different platform-specific createInfo structures, e.g. `VkWin32SurfaceCreateInfoKHR`.
   * - GLFW provides `glfwCreateWindowSurface` handle platform-specific surface creation for us.
   *
   */
  vk::raii::SurfaceKHR surface_ = nullptr;

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
   * @brief Used for command buffers allocation when user does not provide the command pool.
   *
   */
  vk::raii::CommandPool single_time_cmd_pool_ = nullptr;

  // TODO(migoox): allow for creation of multiple queues
  vk::raii::Queue graphics_queue_ = nullptr;
  uint32_t graphics_queue_family_{};
  vk::raii::Queue compute_queue_ = nullptr;
  uint32_t compute_queue_family_{};
  vk::raii::Queue presentation_queue_ = nullptr;
  uint32_t presentation_queue_family_{};
};

}  // namespace eray::vkren
