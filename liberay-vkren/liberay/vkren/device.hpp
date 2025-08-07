#pragma once

#include <liberay/util/logger.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/surface.hpp>
#include <variant>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

/**
 * @brief Simplifies logical device creation and provides additional functions not available in `vk::raii:Device`.
 *
 */
class Device {
 public:
  ERAY_DELETE_COPY(Device)
  ERAY_DEFAULT_MOVE(Device)

  struct InstanceCreationError {};
  struct DebugMessengerCreationError {};
  struct SurfaceCreationError {};
  struct PhysicalDevicePickingError {};
  struct LogicalDeviceCreationError {};

  using CreationError = std::variant<InstanceCreationError, PhysicalDevicePickingError, LogicalDeviceCreationError,
                                     ISurfaceCreator::SurfaceCreationError, DebugMessengerCreationError>;

  struct CreateInfo {
    /**
     * @brief When empty, validation layers are completely disabled, otherwise "VK_EXT_debug_utils" global extension
     * will be automatically requested (no matter if specified in global_extensions explicitly).
     *
     */
    std::span<const char*> validation_layers;

    /**
     * @note If validation layers are not empty, "VK_EXT_debug_utils" is inserted automatically.
     *
     */
    std::span<const char*> global_extensions;

    std::span<const char*> device_extensions;

    /**
     * @brief Implementation of ISurfaceCreator, e.g. for GLFW the creator would call glfwCreateWindowSurface.
     *
     */
    std::unique_ptr<ISurfaceCreator> surface_creator;

    /**
     * @brief If null, a default feature chain that enables dynamic rendering will be used.
     *
     */
    const void* feature_chain = nullptr;

    /**
     * @brief Severity flags used for debug messenger.
     *
     * @note If validation layers are empty, no debug messenger will be set up.
     *
     */
    vk::DebugUtilsMessageSeverityFlagsEXT severity_flags =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

    vk::ApplicationInfo app_info = vk::ApplicationInfo{
        .pApplicationName   = "Vulkan Application",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "No Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = vk::ApiVersion14  //
    };
  };
  static std::expected<Device, CreationError> create(const CreateInfo& info) noexcept;

  vk::raii::Context& ctx() noexcept { return context_; }
  const vk::raii::Context& ctx() const noexcept { return context_; }

  vk::raii::Instance& instance() noexcept { return instance_; }
  const vk::raii::Instance& instance() const noexcept { return instance_; }

  vk::raii::PhysicalDevice& physical_device() noexcept { return physical_device_; }
  const vk::raii::PhysicalDevice& physical_device() const noexcept { return physical_device_; }

  vk::raii::Device& logical_device() noexcept { return device_; }
  const vk::raii::Device& logical_device() const noexcept { return device_; }

  vk::raii::Device& operator->() noexcept { return device_; }
  const vk::raii::Device& operator->() const noexcept { return device_; }

  vk::raii::Device& operator*() noexcept { return device_; }
  const vk::raii::Device& operator*() const noexcept { return device_; }

  uint32_t graphics_queue_family_index() const { return graphics_queue_family_index_; }
  vk::raii::Queue& graphics_queue() noexcept { return graphics_queue_; }
  const vk::raii::Queue& graphics_queue() const noexcept { return graphics_queue_; }

  uint32_t presentation_queue_family_index() const { return presentation_queue_family_index_; }
  vk::raii::Queue& presentation_queue() noexcept { return presentation_queue_; }
  const vk::raii::Queue& presentation_queue() const noexcept { return presentation_queue_; }

 private:
  Device();

  std::expected<void, InstanceCreationError> create_instance(const CreateInfo& info) noexcept;
  std::expected<void, DebugMessengerCreationError> create_debug_messenger(const CreateInfo& info) noexcept;
  std::expected<void, PhysicalDevicePickingError> pick_physical_device(const CreateInfo& info) noexcept;
  std::expected<void, LogicalDeviceCreationError> create_logical_device(const CreateInfo& info) noexcept;

  std::vector<const char*> get_global_extensions(const CreateInfo& info) noexcept;

  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
                                                         vk::DebugUtilsMessageTypeFlagsEXT type,
                                                         const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data,
                                                         void*);

  static auto get_default_feature_chain() {
    return vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                              vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>{
        {},                                                            // vk::PhysicalDeviceFeatures2
        {.synchronization2 = vk::True, .dynamicRendering = vk::True},  // Enable dynamic rendering from Vulkan 1.3
        {.extendedDynamicState = vk::True}  // Enable extended dynamic state from the extension
    };
  }

 private:
  /**
   * @brief Responsible for dynamic loading of the Vulkan library, it's a starting point.
   * It's a starting point for creating other RAII-based Vulkan objects like vk::raii::Instance or vk::raii::Device.
   * e.g. context_.createInstance(...) wraps C-style vkCreateInstance.
   *
   */
  vk::raii::Context context_;

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
   * returns the platform specific Vulkan extensions.
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

  vk::raii::Queue graphics_queue_ = nullptr;
  uint32_t graphics_queue_family_index_{};
  vk::raii::Queue presentation_queue_ = nullptr;
  uint32_t presentation_queue_family_index_{};
};

}  // namespace eray::vkren
