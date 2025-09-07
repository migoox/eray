#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/image_format_helpers.hpp>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

std::uint32_t ImageDescription::find_mip_levels() const {
  // Alternative like this may suffer from the floating-point precision errors:
  // return static_cast<uint32_t>(std::floor(std::log2(std::max({width, height, depth})))) + 1;
  //
  // For that reason, integer math is used instead
  //

  uint32_t largest = std::max({width, height, depth});
  uint32_t levels  = 0;
  while (largest > 0) {
    largest >>= 1;
    levels++;
  }

  return levels;
}

vk::DeviceSize ImageDescription::lod0_size_bytes() const {
  return helper::bytes_per_pixel(format) * width * height * depth * array_layers;
}

vk::DeviceSize ImageDescription::find_full_size_bytes() const {
  if (depth > 1 && array_layers == 1) {
    auto mip_width  = width;
    auto mip_height = height;
    auto mip_depth  = depth;

    vk::DeviceSize buff_size = 0;
    for (auto i = 0U; i < find_mip_levels(); ++i) {
      buff_size += mip_width * mip_height * mip_depth;
      mip_width  = std::max(mip_width / 2U, 1U);
      mip_height = std::max(mip_height / 2U, 1U);
      mip_depth  = std::max(mip_depth / 2U, 1U);
    }

    return buff_size * helper::bytes_per_pixel(format);
  }

  if (array_layers >= 1 && depth == 1) {
    auto mip_width  = width;
    auto mip_height = height;

    vk::DeviceSize buff_size = 0;
    for (auto i = 0U; i < find_mip_levels(); ++i) {
      buff_size += mip_width * mip_height;
      mip_width  = std::max(mip_width / 2U, 1U);
      mip_height = std::max(mip_height / 2U, 1U);
    }

    return buff_size * array_layers * helper::bytes_per_pixel(format);
  }

  assert(false && "At least one of the values: array_layers, depth must be equal 1!");
}

ImageDescription ImageDescription::from(const res::Image& image) {
  return ImageDescription{
      .format       = vk::Format::eR8G8B8A8Srgb,
      .width        = image.width(),
      .height       = image.height(),
      .depth        = 1,
      .array_layers = 1,
  };
}

}  // namespace eray::vkren
