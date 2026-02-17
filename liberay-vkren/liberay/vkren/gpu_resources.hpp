#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstdint>

namespace eray::vkren {

// === Resource Handles ===

template <typename T>
struct GPUResourceHandle {
  using IndexType   = uint32_t;
  using VersionType = uint32_t;

  IndexType _index;
  VersionType _version;

  static constexpr IndexType kInvalidIndex = 0xFFFFFFFF;

  static GPUResourceHandle<T> create(IndexType index, VersionType version) {
    return GPUResourceHandle<T>{._index = index, ._version = version};
  }

  inline bool operator==(const GPUResourceHandle& other) const {
    return _index == other._index && _version == other._version;
  }

  inline IndexType index() const { return _index; }
  inline VersionType version() const { return _version; }
};

using BufferHandle  = GPUResourceHandle<struct BufferTag>;
using ImageHandle   = GPUResourceHandle<struct ImageTag>;
using SamplerHandle = GPUResourceHandle<struct SamplerTag>;

// === Resource creation info structs ===

struct BufferCreateInfo {
  VmaAllocationCreateFlags alloc_flags{0};
  VmaMemoryUsage mem_usage{};
  VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
  VkBufferUsageFlags usage{0};
  VkDeviceSize size_bytes   = 0;
  VkDeviceSize offset_bytes = 0;
  void* initial_data        = nullptr;
  const char* name          = nullptr;

  /**
   * @brief Creates buffer creation info with provided VMA allocation flags, memory usage and buffer usage.
   * @param alloc_flags
   * @param mem_usage
   * @param usage
   * @param size_bytes
   */
  static BufferCreateInfo buffer(VmaAllocationCreateFlags alloc_flags, VmaMemoryUsage mem_usage,
                                 VkBufferUsageFlags usage, VkDeviceSize size_bytes);

  /**
   * @brief Creates buffer creation info for a mappable buffer with provided buffer usage and size. VMA flags are set to
   * VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT and memory usage is set to VMA_MEMORY_USAGE_AUTO.
   * @warning Only use `memcpy` to update the mapped memory if `random_access` is false (it is by default).
   * @param usage
   * @param size_bytes
   * @param random_access If set to true, the buffer will be allocated with VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
   */
  static BufferCreateInfo mappable_buffer(VkBufferUsageFlags usage, VkDeviceSize size_bytes,
                                          bool random_access = false);

  /**
   * @brief Creates buffer creation info for a persistently mapped buffer with provided buffer usage and size. VMA flags
   * are set to VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT and memory
   * usage is set to VMA_MEMORY_USAGE_AUTO.
   * @warning Only use `memcpy` to update the mapped memory if `random_access` is false (it is by default).
   * @param usage
   * @param size_bytes
   * @param random_access If set to true, the buffer will be allocated with VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT.
   */
  static BufferCreateInfo persistently_mapped_buffer(VkBufferUsageFlags usage, VkDeviceSize size_bytes,
                                                     bool random_access = false);

  /**
   * @brief Creates buffer creation info for a device local buffer with provided buffer usage and size. VMA flags are
   * set to 0 and memory usage is set to VMA_MEMORY_USAGE_AUTO.
   * @param usage
   * @param size_bytes
   */
  static BufferCreateInfo device_only_buffer(VkBufferUsageFlags usage, VkDeviceSize size_bytes);

  /**
   * @note If VkBufferUsageFlags does not contain VK_BUFFER_USAGE_TRANSFER_DST_BIT, it will be automatically appended.
   * @param data
   * @param offset_bytes
   */
  BufferCreateInfo& with_initial_data(void* data, VkDeviceSize offset_bytes = 0);

  BufferCreateInfo& with_name(const char* name);
};

struct ImageCreation {
  void* initial_data = nullptr;
  const char* name   = nullptr;
};

}  // namespace eray::vkren