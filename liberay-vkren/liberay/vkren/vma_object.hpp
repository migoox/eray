#pragma once
#include <vma/vk_mem_alloc.h>

#include <liberay/util/hash_combine.hpp>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

struct VmaImage {
  vk::Image vk_image;
  VmaAllocation allocation;

  bool operator==(const VmaImage& other) const noexcept {
    return vk_image == other.vk_image && allocation == other.allocation;
  }
};

struct VmaBuffer {
  vk::Buffer vk_buffer;
  VmaAllocation allocation;

  bool operator==(const VmaBuffer& other) const noexcept {
    return vk_buffer == other.vk_buffer && allocation == other.allocation;
  }
};

}  // namespace eray::vkren

template <>
struct std::hash<eray::vkren::VmaImage> {
  std::size_t operator()(const eray::vkren::VmaImage& img) const noexcept {
    std::size_t h = 0;
    eray::util::hash_combine(h, std::hash<VkImage>{}(static_cast<VkImage>(img.vk_image)));
    eray::util::hash_combine(h, std::hash<void*>{}(img.allocation));
    return h;
  }
};

template <>
struct std::hash<eray::vkren::VmaBuffer> {
  std::size_t operator()(const eray::vkren::VmaBuffer& buf) const noexcept {
    std::size_t h = 0;
    eray::util::hash_combine(h, std::hash<VkBuffer>{}(static_cast<VkBuffer>(buf.vk_buffer)));
    eray::util::hash_combine(h, std::hash<void*>{}(buf.allocation));
    return h;
  }
};
