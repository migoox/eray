#pragma once

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/vma_allocation_manager.hpp>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

/**
 * @brief RAII VMA object. This struct must not outlive the allocator.
 *
 */
template <typename TVmaObjectHandle,
          void (*DeleteCallback)(VmaAllocationManager&, VmaAllocation, TVmaObjectHandle) noexcept>
struct VmaRaiiObject {
  observer_ptr<VmaAllocationManager> _alloc_manager{};
  VmaAllocation _allocation{};
  TVmaObjectHandle _vk_handle{};

  VmaRaiiObject() = delete;
  explicit VmaRaiiObject(std::nullptr_t) {}

  VmaRaiiObject(VmaAllocationManager& alloc_manager, VmaAllocation allocation, TVmaObjectHandle vk_handle)
      : _alloc_manager(&alloc_manager), _allocation(allocation), _vk_handle(vk_handle) {}

  VmaRaiiObject(VmaRaiiObject&& other) noexcept
      : _alloc_manager(other._alloc_manager), _allocation(other._allocation), _vk_handle(other._vk_handle) {
    other._allocation    = nullptr;
    other._alloc_manager = nullptr;
    other._vk_handle     = VK_NULL_HANDLE;
  }

  VmaRaiiObject& operator=(VmaRaiiObject&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    if (_allocation != nullptr && _alloc_manager != nullptr && _vk_handle != VK_NULL_HANDLE) {
      DeleteCallback(*_alloc_manager, _allocation, _vk_handle);
    }

    _allocation    = other._allocation;
    _alloc_manager = other._alloc_manager;
    _vk_handle     = other._vk_handle;

    other._allocation    = nullptr;
    other._alloc_manager = nullptr;
    other._vk_handle     = VK_NULL_HANDLE;

    return *this;
  }

  VmaRaiiObject(const VmaRaiiObject& other)            = delete;
  VmaRaiiObject& operator=(const VmaRaiiObject& other) = delete;

  const VmaAllocationManager& allocation_manager() const { return *_alloc_manager; }
  VmaAllocationManager& allocation_manager() { return *_alloc_manager; }

  VmaAllocationInfo alloc_info() const {
    VmaAllocationInfo info;
    vmaGetAllocationInfo(_alloc_manager->allocator(), _allocation, &info);
    return info;
  }

  ~VmaRaiiObject() {
    if (_allocation != nullptr && _alloc_manager != nullptr && _vk_handle != VK_NULL_HANDLE) {
      DeleteCallback(*_alloc_manager, _allocation, _vk_handle);
    }
  }
};

using VmaRaiiBuffer = VmaRaiiObject<vk::Buffer, [](VmaAllocationManager& alloc_manager, VmaAllocation allocation,
                                                   vk::Buffer buffer) noexcept {
  alloc_manager.delete_buffer(VmaBuffer{.vk_buffer = buffer, .allocation = allocation});
}>;

using VmaRaiiImage = VmaRaiiObject<vk::Image, [](VmaAllocationManager& alloc_manager, VmaAllocation allocation,
                                                 vk::Image image) noexcept {
  alloc_manager.delete_image(VmaImage{.vk_image = image, .allocation = allocation});
}>;

}  // namespace eray::vkren
