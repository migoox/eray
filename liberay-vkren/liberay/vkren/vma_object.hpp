#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <liberay/util/hash_combine.hpp>

namespace eray::vkren {


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
