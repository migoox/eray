#include <liberay/vkren/gpu_resources.hpp>

namespace eray::vkren {

BufferCreateInfo BufferCreateInfo::buffer(VmaAllocationCreateFlags alloc_flags, VmaMemoryUsage mem_usage,
                                          VkBufferUsageFlags usage, VkDeviceSize size_bytes) {
  return BufferCreateInfo{
      .alloc_flags  = alloc_flags,
      .mem_usage    = mem_usage,
      .sharing_mode = VK_SHARING_MODE_EXCLUSIVE,
      .usage        = usage,
      .size_bytes   = size_bytes,
      .offset_bytes = 0,
      .initial_data = nullptr,
      .name         = nullptr,
  };
}

BufferCreateInfo BufferCreateInfo::mappable_buffer(VkBufferUsageFlags usage, VkDeviceSize size_bytes,
                                                   bool random_access) {
  if (random_access) {
    return buffer(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VMA_MEMORY_USAGE_AUTO, usage, size_bytes);
  } else {
    return buffer(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO, usage, size_bytes);
  }
}

BufferCreateInfo BufferCreateInfo::persistently_mapped_buffer(VkBufferUsageFlags usage, VkDeviceSize size_bytes,
                                                              bool random_access) {
  if (random_access) {
    return buffer(VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                  VMA_MEMORY_USAGE_AUTO, usage, size_bytes);
  } else {
    return buffer(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO, usage, size_bytes);
  }
}

BufferCreateInfo BufferCreateInfo::device_only_buffer(VkBufferUsageFlags usage, VkDeviceSize size_bytes) {
  return buffer(0, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, usage, size_bytes);
}

BufferCreateInfo BufferCreateInfo::staging_buffer(VkDeviceSize size_bytes, bool transfer_dst) {
  if (transfer_dst) {
    return buffer(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size_bytes);
  } else {
    return buffer(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size_bytes);
  }
}

BufferCreateInfo& BufferCreateInfo::with_initial_data(void* data, VkDeviceSize offset) {
  this->initial_data = data;
  this->offset_bytes = offset;
  this->usage |=
      VK_BUFFER_USAGE_TRANSFER_DST_BIT;  // Ensure the buffer can be a transfer destination for the initial data upload.
}

BufferCreateInfo& BufferCreateInfo::with_name(const char* name) { this->name = name; }

}  // namespace eray::vkren