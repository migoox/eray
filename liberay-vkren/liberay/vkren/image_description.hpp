#pragma once

#include <liberay/vkren/image_format_helpers.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace eray::vkren {

/**
 * @brief Either `depth` or `array_layers` is equal to 1.
 *
 */
struct ImageDescription {
  vk::Format format;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t depth;
  std::uint32_t array_layers;

  static ImageDescription image2d_desc(vk::Format format, std::uint32_t width, std::uint32_t height,
                                       std::uint32_t array_layers = 1) {
    return ImageDescription{
        .format       = format,
        .width        = width,
        .height       = height,
        .depth        = 1,
        .array_layers = array_layers,
    };
  }

  static ImageDescription image3d_desc(vk::Format format, std::uint32_t width, std::uint32_t height,
                                       std::uint32_t depth) {
    return ImageDescription{
        .format       = format,
        .width        = width,
        .height       = height,
        .depth        = depth,
        .array_layers = 1,  // for 3d images, layers count must be 1
    };
  }

  /**
   * @brief Calculates the mip levels of the image.
   *
   * @return std::uint32_t
   */
  std::uint32_t find_mip_levels() const;

  /**
   * @brief Size of the image in level of detail 0 (width*height*depth*array_layers*bytes_per_pixel).
   *
   * @return vk::DeviceSize
   */
  vk::DeviceSize lod0_size_bytes() const;

  /**
   * @brief Calculates the full size in bytes, includes mipmaps and layers.
   *
   * @return vk::DeviceSize
   */
  vk::DeviceSize find_full_size_bytes() const;

  vk::ImageViewType image_view_type() const { return depth > 1 ? vk::ImageViewType::e3D : vk::ImageViewType::e2D; }
};

}  // namespace eray::vkren
