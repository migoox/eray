#pragma once
#include <vulkan/vulkan.h>

#include <cstdint>
#include <liberay/vkren/gpu_enum.hpp>

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
  BufferMemoryType memory_type;
  VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
  VkBufferUsageFlags usage{0};

  VkDeviceSize size_bytes   = 0;        // optional
  VkDeviceSize offset_bytes = 0;        // optional
  void* initial_data        = nullptr;  // optional
  const char* name          = nullptr;  // optional

  /**
   * @brief Initializes buffer creation info with mandatory fields.
   * @param alloc_flags
   * @param mem_usage
   * @param usage
   * @param size_bytes
   */
  static BufferCreateInfo create(BufferMemoryType memory_type, VkBufferUsageFlags usage, VkDeviceSize size_bytes);

  BufferCreateInfo& with_initial_data(void* data, VkDeviceSize offset_bytes = 0);
  BufferCreateInfo& with_sharing_mode_concurrent();
  BufferCreateInfo& with_name(const char* name);
};

struct ImageCreation {
  void* initial_data = nullptr;
  const char* name   = nullptr;
};

}  // namespace eray::vkren