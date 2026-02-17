#include <liberay/vkren/gpu_resource_manager.hpp>

namespace eray::vkren {

[[nodiscard]] Result<BufferHandle, Error> ResourceManager::create_buffer(const BufferCreateInfo& create_info) noexcept {
  auto buff_create_info = VkBufferCreateInfo{
      .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size        = create_info.size_bytes,
      .usage       = create_info.usage,
      .sharingMode = create_info.sharing_mode,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = create_info.mem_usage;
  alloc_create_info.flags                   = create_info.alloc_flags;

  VmaAllocationInfo alloc_info;
  VkBuffer buffer     = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;
  VkResult res = vmaCreateBuffer(allocator_, &buff_create_info, &alloc_create_info, &buffer, &alloc, &alloc_info);

  VmaBuffer vma_buffer{.vk_buffer = buffer, .allocation = alloc};

}

}  // namespace eray::vkren
