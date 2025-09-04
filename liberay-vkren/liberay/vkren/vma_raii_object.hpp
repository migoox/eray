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
template <typename TVMAObject, void (*DeleteCallback)(VmaAllocator, VmaAllocation, TVMAObject) noexcept>
struct VMARaiiObject {
  VmaAllocator _allocator{};
  VmaAllocation _allocation{};
  TVMAObject _handle{};

  VMARaiiObject(VmaAllocator allocator, VmaAllocation allocation, TVMAObject handle)
      : _allocator(allocator), _allocation(allocation), _handle(handle) {}

  VMARaiiObject(VMARaiiObject&& other) noexcept
      : _allocator(other._allocator), _allocation(other._allocation), _handle(other._handle) {
    other._allocation = nullptr;
    other._allocator  = nullptr;
    other._handle     = VK_NULL_HANDLE;
  }

  VMARaiiObject& operator=(VMARaiiObject&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    if (_allocation != nullptr && _allocator != nullptr && _handle != VK_NULL_HANDLE) {
      DeleteCallback(_allocator, _allocation, _handle);
    }

    _allocation = other._allocation;
    _allocator  = other._allocator;
    _handle     = other._handle;

    other._allocation = nullptr;
    other._allocator  = nullptr;
    other._handle     = VK_NULL_HANDLE;

    return *this;
  }

  VMARaiiObject(const VMARaiiObject& other)            = delete;
  VMARaiiObject& operator=(const VMARaiiObject& other) = delete;

  VmaAllocationInfo alloc_info() const {
    VmaAllocationInfo info;
    vmaGetAllocationInfo(_allocator, _allocation, &info);
    return info;
  }

  ~VMARaiiObject() {
    if (_allocation != nullptr && _allocator != nullptr && _handle != VK_NULL_HANDLE) {
      DeleteCallback(_allocator, _allocation, _handle);
    }
  }
};

using VMARaiiBuffer =
    VMARaiiObject<VkBuffer, [](VmaAllocator allocator, VmaAllocation allocation, VkBuffer buffer) noexcept {
      vmaDestroyBuffer(allocator, buffer, allocation);
    }>;

using VMARaiiImage =
    VMARaiiObject<VkImage, [](VmaAllocator allocator, VmaAllocation allocation, VkImage image) noexcept {
      vmaDestroyImage(allocator, image, allocation);
    }>;

}  // namespace eray::vkren
