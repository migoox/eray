#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace eray::vkren {

struct ImageDescription {
  vk::Format format;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t mip_levels;
};

}  // namespace eray::vkren
