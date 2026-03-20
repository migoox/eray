#include <liberay/vkren/gpu_resources.hpp>

namespace eray::vkren {

BufferCreateInfo BufferCreateInfo::create(BufferMemoryType memory_type, VkBufferUsageFlags usage, VkDeviceSize size_bytes) {
  return BufferCreateInfo{
      .memory_type = memory_type,
      .sharing_mode = VK_SHARING_MODE_EXCLUSIVE,
      .usage        = usage,
      .size_bytes   = size_bytes,
      .offset_bytes = 0,
      .initial_data = nullptr,
      .name         = nullptr,
  };
}

BufferCreateInfo& BufferCreateInfo::with_initial_data(void* data, VkDeviceSize offset) {
  this->initial_data = data;
  this->offset_bytes = offset;
}

BufferCreateInfo& BufferCreateInfo::with_sharing_mode_concurrent() { this->sharing_mode = VK_SHARING_MODE_CONCURRENT; }

BufferCreateInfo& BufferCreateInfo::with_name(const char* name) { this->name = name; }

}  // namespace eray::vkren