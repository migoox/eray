#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <liberay/util/ruleof.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/image.hpp>
#include <memory>
#include <vulkan/vulkan.hpp>

#include "liberay/os/window/window.hpp"

namespace eray::vkren {

class SwapChain {
 public:
  /**
   * @brief Creates uninitialized empty SwapChain. Useful for postponed creation, usually when the SwapChain is a class
   * member.
   *
   * @warning This constructor is unsafe. It's programmer responsibility to overwrite the empty swap chain with proper
   * initialized one.
   *
   */
  explicit SwapChain(std::nullptr_t);

  SwapChain(const SwapChain&)                = delete;
  SwapChain(SwapChain&&) noexcept            = default;
  SwapChain& operator=(const SwapChain&)     = delete;
  SwapChain& operator=(SwapChain&&) noexcept = default;

  static Result<SwapChain, Error> create(Device& device, std::shared_ptr<os::Window>,
                                         vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) noexcept;

  vk::raii::SwapchainKHR* operator->() noexcept { return &swap_chain_; }
  const vk::raii::SwapchainKHR* operator->() const noexcept { return &swap_chain_; }

  vk::raii::SwapchainKHR& operator*() noexcept { return swap_chain_; }
  const vk::raii::SwapchainKHR& operator*() const noexcept { return swap_chain_; }

  const std::vector<vk::Image>& images() const { return images_; }
  vk::Image depth_stencil_attachment_image() const { return depth_stencil_image_.image(); }
  vk::Image color_attachment_image() const { return color_image_.image(); }

  const std::vector<vk::raii::ImageView>& image_views() { return image_views_; }
  vk::ImageView depth_stencil_attachment_image_view() const { return depth_stencil_image_view_; }

  /**
   * @brief Color attachment for MSAA that can be used in render pass multisample resolve operation.
   * See https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#renderpass-resolve-operations.
   *
   * @return const vk::raii::ImageView&
   */
  vk::ImageView color_attachment_image_view() const { return color_image_view_; }

  vk::Format image_format() { return format_; }

  vk::Format color_attachment_format() { return format_; }
  vk::Format depth_stencil_attachment_format() { return depth_stencil_format_; }

  const vk::Extent2D& extent() { return extent_; }

  /**
   * @brief Starts the rendering queue and sets up attachments and swap chain for rendering.
   *
   * @param cmd_buff
   * @param image_index
   * @param clear_color
   * @param clear_depth_stencil
   */
  void begin_rendering(const vk::raii::CommandBuffer& cmd_buff, uint32_t image_index,
                       vk::ClearColorValue clear_color                = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F),
                       vk::ClearDepthStencilValue clear_depth_stencil = vk::ClearDepthStencilValue(1.0F, 0));
  /**
   * @brief Sets up attachments for presentation and finishes the rendering queue.
   *
   * @param cmd_buff
   * @param image_index
   * @param clear_color
   * @param clear_depth_stencil
   */
  void end_rendering(const vk::raii::CommandBuffer& cmd_buff, uint32_t image_index);

  /**
   * @brief Allows to destroy the swap chain explicitly. Example use case: Swap chain must be destroyed before
   * destroying the GLFW window.
   *
   */
  void cleanup();

  vk::SampleCountFlagBits msaa_sample_count() const { return msaa_sample_count_; }

  bool msaa_enabled() const { return msaa_sample_count_ != vk::SampleCountFlagBits::e1; }

  struct AcquireResult {
    enum class Status : uint8_t {
      Success = 0,
      Resized = 1,
    };

    Status status;

    /**
     * @brief Only valid when status == Success
     *
     */
    uint32_t image_index;
  };

  /**
   * @brief Calls `vkAcquireNextImageKHR` and resize the swap chain if necessary (if swap chain gets resized returns
   * std::nullopt).
   *
   * @param timeout
   * @param semaphore
   * @param fence
   * @return Result<uint32_t, Error>
   */
  [[nodiscard]] Result<AcquireResult, Error> acquire_next_image(uint64_t timeout,
                                                                vk::Semaphore semaphore = VK_NULL_HANDLE,
                                                                vk::Fence fence         = VK_NULL_HANDLE);

  Result<void, Error> present_image(vk::PresentInfoKHR present_info);

  Result<void, Error> recreate();

  /**
   * @brief Returns a window to which swap chain presents its images.
   *
   * @return const os::Window&
   */
  const os::Window& window() const { return *window_; }

 private:
  SwapChain() = default;

  Result<void, Error> create_swap_chain(vkren::Device& device, uint32_t width, uint32_t height) noexcept;
  Result<void, Error> create_image_views(vkren::Device& device) noexcept;
  Result<void, Error> create_color_buffer(vkren::Device& device) noexcept;
  Result<void, Error> create_depth_stencil_buffer(vkren::Device& device) noexcept;

  Result<vk::Format, Error> find_supported_depth_stencil_format(const Device& device,
                                                                const std::vector<vk::Format>& candidates,
                                                                vk::ImageTiling tiling,
                                                                vk::FormatFeatureFlags features);

  static vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats);
  static vk::PresentModeKHR choose_swap_presentMode(const std::vector<vk::PresentModeKHR>& available_present_modes);

 private:
  /**
   * @brief Vulkan does not provide a "default framebuffuer". Hence it requires an infrastructure that will own the
   * buffers we will render to before we visualize them on the screen. This infrastructure is known as the swap chain.
   *
   * The swap is a queue of images that are waiting to be presented to the screen. The general purpose of the swap
   * chain is to synchronize the presentation of images with the refresh rate of teh screen.
   *
   */
  vk::raii::SwapchainKHR swap_chain_ = vk::raii::SwapchainKHR(nullptr);

  std::vector<vk::Image> images_;

  /**
   * @brief An image view DESCRIBES HOW TO ACCESS THE IMAGE and which part of the image to access, for example, if it
   * should be treated as a 2D texture depth texture without any mipmapping levels.
   *
   */
  std::vector<vk::raii::ImageView> image_views_;

  /**
   * @brief Handle to a color buffer attachment.
   *
   */
  vkren::ImageResource color_image_;
  vk::raii::ImageView color_image_view_ = nullptr;

  /**
   * @brief Handle to a depth buffer attachment.
   *
   */
  vkren::ImageResource depth_stencil_image_;
  vk::raii::ImageView depth_stencil_image_view_ = nullptr;
  vk::Format depth_stencil_format_              = vk::Format::eUndefined;

  /**
   * @brief Describes the format e.g. RGBA.
   *
   */
  vk::Format format_ = vk::Format::eUndefined;

  /**
   * @brief Describes the dimensions of the swap chain.
   *
   */
  vk::Extent2D extent_{};

  observer_ptr<Device> p_device_{};

  vk::SampleCountFlagBits msaa_sample_count_ = vk::SampleCountFlagBits::e1;

  std::shared_ptr<const os::Window> window_;

  bool framebuffer_resized_{};
};

}  // namespace eray::vkren
