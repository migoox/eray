#pragma once

#include <cstddef>
#include <liberay/util/ruleof.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <variant>
#include <vulkan/vulkan_raii.hpp>

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
  explicit SwapChain(std::nullptr_t) {}

  ERAY_DELETE_COPY(SwapChain)
  ERAY_DEFAULT_MOVE(SwapChain)

  struct SwapChainCreationError {};
  struct ImageViewsCreationError {};

  using CreationError = std::variant<SwapChainCreationError, ImageViewsCreationError>;

  static Result<SwapChain, CreationError> create(const Device& device, uint32_t width, uint32_t height) noexcept;

  vk::raii::SwapchainKHR* operator->() noexcept { return &swap_chain_; }
  const vk::raii::SwapchainKHR* operator->() const noexcept { return &swap_chain_; }

  vk::raii::SwapchainKHR& operator*() noexcept { return swap_chain_; }
  const vk::raii::SwapchainKHR& operator*() const noexcept { return swap_chain_; }

  const std::vector<vk::Image>& images() { return images_; }

  const std::vector<vk::raii::ImageView>& image_views() { return image_views_; }

  vk::Format format() { return format_; }

  const vk::Extent2D& extent() { return extent_; }

  /**
   * @brief
   *
   * @param device_
   */
  Result<void, SwapChain::CreationError> recreate(const Device& device, uint32_t width, uint32_t height);

  /**
   * @brief Allows to destroy the swap chain explicitly. Example use case: Swap chain must be destroyed before
   * destroying the GLFW window.
   *
   */
  void cleanup();

 private:
  SwapChain() = default;

  Result<void, SwapChainCreationError> create_swap_chain(const vkren::Device& device, uint32_t width,
                                                         uint32_t height) noexcept;

  Result<void, ImageViewsCreationError> create_image_views(const vkren::Device& device) noexcept;

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

  /**
   * @brief Stores handles to the Swap chain images.
   *
   */
  std::vector<vk::Image> images_;

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

  /**
   * @brief An image view DESCRIBES HOW TO ACCESS THE IMAGE and which part of the image to access, for example, if it
   * should be treated as a 2D texture depth texture without any mipmapping levels.
   *
   */
  std::vector<vk::raii::ImageView> image_views_;
};

}  // namespace eray::vkren
