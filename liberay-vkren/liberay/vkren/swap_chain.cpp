#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/variant_match.hpp>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

Result<SwapChain, Error> SwapChain::create(const Device& device, uint32_t width, uint32_t height,
                                           vk::SampleCountFlagBits sample_count) noexcept {
  auto swap_chain               = SwapChain();
  swap_chain.msaa_sample_count_ = sample_count;
  TRY(swap_chain.create_swap_chain(device, width, height));
  TRY(swap_chain.create_image_views(device));
  TRY(swap_chain.create_color_buffer(device));
  TRY(swap_chain.create_depth_stencil_buffer(device));
  return swap_chain;
}

Result<void, Error> SwapChain::create_swap_chain(const Device& device, uint32_t width, uint32_t height) noexcept {
  // Surface formats (pixel format, e.g. B8G8R8A8, color space e.g. SRGB)
  auto available_formats       = device.physical_device().getSurfaceFormatsKHR(device.surface());
  auto available_present_modes = device.physical_device().getSurfacePresentModesKHR(device.surface());

  if (available_formats.empty() || available_present_modes.empty()) {
    eray::util::Logger::err(
        "The physical device's swap chain support is not sufficient. Required at least one available format and at "
        "least one presentation mode.");
    return std::unexpected(Error{
        .msg  = "Required at least one format and at least one presentation mode.",
        .code = ErrorCode::PhysicalDeviceNotSufficient{},
    });
  }

  auto swap_surface_format = choose_swap_surface_format(available_formats);

  // Presentation mode represents the actual conditions for showing imgaes to the screen:
  //
  //  - VK_PRESENT_MODE_IMMEDIATE_KHR:    images are transferred to the screen right away -- tearing
  //
  //  - VK_PRESENT_MODE_FIFO_KHR:         swap chain uses FIFO queue, if the queue is full the program waits -- VSync
  //
  //  - VK_PRESENT_MODE_FIFO_RELAXED_KHR: similar to the previous one, if the app is late and the queue was empty, the
  //                                      image is send right away
  //
  //  - VK_PRESENT_MODE_MAILBOX_KHR:      another variant of the second mode., if the queue is full, instead of
  //                                      blocking, the images that are already queued are replaced with the new ones
  //                                      fewer latency, avoids tearing issues -- triple buffering
  //
  // Note: Only the VK_PRESENT_MODE_MAILBOX_KHR is guaranteed to be available

  auto swap_present_mode = choose_swap_presentMode(available_present_modes);

  // Basic Surface capabilities (min/max number of images in the swap chain, min/max width and height of images)
  auto surface_capabilities = device.physical_device().getSurfaceCapabilitiesKHR(device.surface());

  // Swap extend is the resolution of the swap chain images, and it's almost always exactly equal to the resolution
  // of the window that we're drawing to in pixels.
  auto swap_extent = vk::Extent2D{
      .width = std::clamp(width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width),
      .height =
          std::clamp(height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height),

  };

  // It is recommended to request at least one more image than the minimum
  auto min_img_count = std::max(3U, surface_capabilities.minImageCount + 1);
  if (surface_capabilities.maxImageCount > 0 && min_img_count > surface_capabilities.maxImageCount) {
    // 0 is a special value that means that there is no maximum

    min_img_count = surface_capabilities.maxImageCount;
  }

  auto swap_chain_info = vk::SwapchainCreateInfoKHR{
      // Almost always left as default
      .flags = vk::SwapchainCreateFlagsKHR(),

      // Window surface on which the swap chain will present images
      .surface = device.surface(),  //

      // Minimum number of images (image buffers). More images reduce the risk of waiting for the GPU to finish
      // rendering, which improves performance
      .minImageCount = min_img_count,  //

      .imageFormat     = swap_surface_format.format,      //
      .imageColorSpace = swap_surface_format.colorSpace,  //
      .imageExtent     = swap_extent,                     //

      // Number of layers each image consists of (unless stereoscopic 3D app is developed it should be 1)
      .imageArrayLayers = 1,  //

      // Kind of images used in the swap chain (it's a bitfield, you can e.g. attach depth and stencil buffers)
      // Also you can render images to a separate image and perform post-processing
      // (VK_IMAGE_USAGE_TRANSFER_DST_BIT).
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,  //

      // We can specify that a certain transform should be applied to images in the swap chain if it is supported,
      // for example 90-degree clockwise rotation or horizontal flip. We specify no transform by using
      // surface_capabilities.currentTransform.
      .preTransform = surface_capabilities.currentTransform,  //

      // Value indicating the alpha compositing mode to use when this surface is composited together with other
      // surfaces on certain window systems
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,  //

      .presentMode = swap_present_mode,  //

      // Applications should set this value to VK_TRUE if they do not expect to read back the content of
      // presentable images before presenting them or after reacquiring them, and if their fragment shaders do not
      // have any side effects that require them to run for all pixels in the presentable image.
      //
      // If the clipped is VK_TRUE, then that means that we don't care about the color of pixels that are
      // obscured, for example, because another window is in front of them => better performance.
      .clipped = vk::True,  //

      // In Vulkan, it's possible that your swap chain becomes invalid or unoptimized while your app is running,
      // e.g. when window gets resized. IN SUCH A CASE THE SWAP CHAIN NEEDS TO BE RECREATED FROM SCRATCH, and a
      // reference to the old one must be specified here.
      .oldSwapchain = VK_NULL_HANDLE,
  };

  // We need to specify how to handle swap chain images that will be used across multiple queue families. That will be
  // the case if graphics and present queue families are different.
  uint32_t indices[] = {device.graphics_queue_family(), device.presentation_queue_family()};

  // There are 2 ways to handle image ownership for queues:
  //  - VK_SHARING_MODE_EXCLUSIVE: Images can be used across multiple queue families without explicit ownership
  //  transfers.
  //  - VK_SHARING_MODE_CONCURRENT: The image is owned by one queue family at a time, and ownership must be explicitly
  //  transferred before
  // using it in another queue family. The best performance.

  if (device.graphics_queue_family() != device.presentation_queue_family()) {
    // Multiple queues -> VK_SHARING_MODE_CONCURRENT to avoid ownership transfers and simplify the code. We are paying
    // a performance cost here.
    swap_chain_info.imageSharingMode = vk::SharingMode::eConcurrent;

    // Specify queues that will share the image ownership
    swap_chain_info.queueFamilyIndexCount = 2;
    swap_chain_info.pQueueFamilyIndices   = indices;
  } else {
    // One queue -> VK_SHARING_MODE_EXCLUSIVE
    swap_chain_info.imageSharingMode = vk::SharingMode::eExclusive;

    // No need to specify which queues share the image ownership
    swap_chain_info.queueFamilyIndexCount = 0;
    swap_chain_info.pQueueFamilyIndices   = nullptr;
  }

  if (auto result = device->createSwapchainKHR(swap_chain_info)) {
    swap_chain_ = std::move(*result);
  } else {
    eray::util::Logger::err("Failed to create a swap chain: {}", vk::to_string(result.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan Swap Chain creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = result.error(),
    });
  }
  images_ = swap_chain_.getImages();
  format_ = swap_surface_format.format;
  extent_ = swap_extent;

  return {};
}

Result<void, Error> SwapChain::create_image_views(const vkren::Device& device) noexcept {
  image_views_.clear();

  auto image_view_info =
      vk::ImageViewCreateInfo{.viewType = vk::ImageViewType::e2D,
                              .format   = format_,

                              // You can map some channels onto the others. We stick to defaults here.
                              .components =
                                  {
                                      .r = vk::ComponentSwizzle::eIdentity,
                                      .g = vk::ComponentSwizzle::eIdentity,
                                      .b = vk::ComponentSwizzle::eIdentity,
                                      .a = vk::ComponentSwizzle::eIdentity,
                                  },

                              // Describes what the image's purpose is and which part of the image should be accessed.
                              // The images here will be used as color targets with no mipmapping levels and
                              // without any multiple layers
                              .subresourceRange = vk::ImageSubresourceRange{
                                  .aspectMask     = vk::ImageAspectFlagBits::eColor,
                                  .baseMipLevel   = 0,
                                  .levelCount     = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount     = 1  //
                              }};

  for (auto image : images_) {
    image_view_info.image = image;
    if (auto result = device->createImageView(image_view_info)) {
      image_views_.emplace_back(std::move(*result));
    } else {
      eray::util::Logger::err("Failed to create a swap chain image view: {}", vk::to_string(result.error()));
      return std::unexpected(Error{
          .msg     = "Swap Chain color attachment Image View creation failure",
          .code    = ErrorCode::VulkanObjectCreationFailure{},
          .vk_code = result.error(),
      });
    }
  }

  return {};
}

Result<void, Error> SwapChain::create_color_buffer(const vkren::Device& device) noexcept {
  auto img_opt = ImageResource::create_color_attachment_image(
      device, ImageDescription::image2d_desc(color_attachment_format(), extent_.width, extent_.height),
      msaa_sample_count_);
  if (!img_opt) {
    util::Logger::err("Could not create an image resource for color attachment");
    return std::unexpected(img_opt.error());
  }
  color_image_ = std::move(*img_opt);

  auto view_opt = color_image_.create_image_view(vk::ImageAspectFlagBits::eColor);
  if (!view_opt) {
    util::Logger::err("Could not create image view for color attachment");
    return std::unexpected(view_opt.error());
  }
  color_image_view_ = std::move(*view_opt);

  return {};
}

Result<void, Error> SwapChain::create_depth_stencil_buffer(const vkren::Device& device) noexcept {
  auto format_opt = find_supported_depth_stencil_format(
      device, {vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
  if (!format_opt) {
    util::Logger::err("Could not create a depth buffer as the requested format is not supported");
    return std::unexpected(format_opt.error());
  }
  depth_stencil_format_ = *format_opt;

  auto img_opt = ImageResource::create_depth_stencil_attachment_image(
      device, ImageDescription::image2d_desc(depth_stencil_format_, extent_.width, extent_.height), msaa_sample_count_);
  if (!img_opt) {
    util::Logger::err("Could not create an image resource for depth buffer");
    return std::unexpected(img_opt.error());
  }

  depth_stencil_image_ = std::move(*img_opt);

  auto view_opt = depth_stencil_image_.create_image_view(vk::ImageAspectFlagBits::eDepth);
  if (!view_opt) {
    util::Logger::err("Could not create image view for depth buffer");
    return std::unexpected(view_opt.error());
  }
  depth_stencil_image_view_ = std::move(*view_opt);

  return {};
}

Result<vk::Format, Error> SwapChain::find_supported_depth_stencil_format(const Device& device,
                                                                         const std::vector<vk::Format>& candidates,
                                                                         vk::ImageTiling tiling,
                                                                         vk::FormatFeatureFlags features) {
  for (const auto format : candidates) {
    auto props = device.physical_device().getFormatProperties(format);
    if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
      return format;
    }
    if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
      return format;
    }
  }

  util::Logger::err("Physical device does not support any of the requested depth buffer formats");
  return std::unexpected(Error{
      .msg  = "Depth buffer formats are not supported",
      .code = ErrorCode::PhysicalDeviceNotSufficient{},
  });
}

vk::SurfaceFormatKHR SwapChain::choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
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

vk::PresentModeKHR SwapChain::choose_swap_presentMode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
  auto mode_it = std::ranges::find_if(available_present_modes, [](const auto& mode) {
    return mode ==
           vk::PresentModeKHR::eMailbox;  // Note: good if energy usage is not a concern, avoid for mobile devices
  });

  if (mode_it != available_present_modes.end()) {
    return *mode_it;
  }

  return vk::PresentModeKHR::eFifo;
}

Result<void, Error> SwapChain::recreate(const Device& device_, uint32_t width, uint32_t height) {
  device_->waitIdle();

  cleanup();
  if (auto result = create_swap_chain(device_, width, height); !result) {
    eray::util::Logger::err("Could not recreate a swap chain: Swap chain creation failed.");
    return std::unexpected(result.error());
  }
  if (auto result = create_image_views(device_); !result) {
    eray::util::Logger::err("Could not recreate a swap chain: Image views creation failed.");
    return std::unexpected(result.error());
  }
  if (auto result = create_color_buffer(device_); !result) {
    eray::util::Logger::err("Could not recreate a swap chain: color buffer attachment creation failed.");
    return std::unexpected(result.error());
  }
  if (auto result = create_depth_stencil_buffer(device_); !result) {
    eray::util::Logger::err("Could not recreate a swap chain: depth buffer attachment creation failed.");
    return std::unexpected(result.error());
  }

  return {};
}

void SwapChain::cleanup() {
  image_views_.clear();
  swap_chain_ = nullptr;
}

}  // namespace eray::vkren
