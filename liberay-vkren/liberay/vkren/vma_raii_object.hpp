#pragma once

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

/**
 * @brief RAII VMA object. This struct must not outlive the allocator.
 *
 */
template <typename TVMAObject, void (*DeleteCallback)(VmaAllocator, VmaAllocation, TVMAObject)>
struct VMARaiiObject {
  VmaAllocator _allocator{};
  VmaAllocation _allocation{};
  TVMAObject _handle{};

  VMARaiiObject(VmaAllocator allocator, VmaAllocation allocation, TVMAObject buffer)
      : _allocator(allocator), _allocation(allocation), _handle(buffer) {}

  VMARaiiObject(VMARaiiObject&& other) noexcept
      : _allocator(other._allocator), _allocation(other._allocation), _handle(other._handle) {
    other._allocation = VK_NULL_HANDLE;
    other._handle     = VK_NULL_HANDLE;
    other._allocator  = VK_NULL_HANDLE;
  }

  VMARaiiObject& operator=(VMARaiiObject&& other) noexcept {
    _allocation = other._allocation;
    _handle     = other._handle;
    _allocator  = other._allocator;

    other._allocation = VK_NULL_HANDLE;
    other._handle     = VK_NULL_HANDLE;
    other._allocator  = VK_NULL_HANDLE;

    return *this;
  }

  VMARaiiObject& operator=(const VMARaiiObject& other) = delete;

  ~VMARaiiObject() {
    if (_allocation != VK_NULL_HANDLE && _handle != VK_NULL_HANDLE && _allocator != VK_NULL_HANDLE) {
      DeleteCallback(_allocator, _handle, _allocation);
    }
  }
};

using VMARaiiBuffer = VMARaiiObject<VkBuffer, [](VmaAllocator allocator, VmaAllocation allocation, VkBuffer buffer) {
  vmaDestroyBuffer(allocator, buffer, allocation);
}>;

using VMARaiiImage = VMARaiiObject<VkImage, [](VmaAllocator allocator, VmaAllocation allocation, VkImage image) {
  vmaDestroyImage(allocator, image, allocation);
}>;

}  // namespace eray::vkren
