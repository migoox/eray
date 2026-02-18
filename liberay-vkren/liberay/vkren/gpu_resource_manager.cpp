#include <liberay/vkren/device.hpp>
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

  if (res != VK_SUCCESS) {
    return std::unexpected(Error{
        .msg     = "Failed to create buffer",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = vk::Result(res),
    });
  }

  bool persistently_mapped = (create_info.alloc_flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) != 0;
  if (!alloc_info.pMappedData && persistently_mapped) {
    vmaDestroyBuffer(allocator_, buffer, alloc);
    return std::unexpected(Error{
        .msg  = "Persistent mapping failed: allocation did not provide pMappedData",
        .code = ErrorCode::VulkanObjectCreationFailure{},
    });
  }

  VkMemoryPropertyFlags mem_prop_flags = 0;
  vmaGetAllocationMemoryProperties(allocator_, alloc, &mem_prop_flags);
  bool mappable_flag = false;
  if (mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    mappable_flag = true;
  }

  auto handle = buffer_pool_.create_entry(BufferResourceEntry{
      .buffer     = buffer,
      .allocation = alloc,
      .size_bytes = create_info.size_bytes,
      .mapping    = alloc_info.pMappedData,
      .usage      = create_info.usage,
      .mappable   = mappable_flag,
  });

  if (create_info.initial_data != nullptr && create_info.size_bytes != 0) {
    this->buffer_write(handle, create_info.initial_data, create_info.size_bytes, create_info.offset_bytes);
  }

  return handle;
}

Result<void, Error> ResourceManager::buffer_write(BufferHandle dst, void* src_data, VkDeviceSize src_size_bytes,
                                                  VkDeviceSize dst_offset_bytes, VkDeviceSize src_offset_bytes) const {
  const auto* dst_entry = buffer_pool_.get_entry(dst);

  // == Bounds checking ==
  if (dst_entry == nullptr) {
    util::panic("Invalid buffer handle");
  }
  if (dst_offset_bytes >= dst_entry->size_bytes) {
    util::panic("dst_offset_bytes exceeds or equals the source buffer size");
  }
  if (src_offset_bytes >= src_size_bytes) {
    util::panic("src_offset_bytes exceeds or equals the source size");
  }
  if (src_size_bytes == VK_WHOLE_SIZE) {
    util::panic(
        "buffer_write() does not support size_bytes == VK_WHOLE_SIZE, because it needs to check the bounds of the "
        "write operation. Specify the size explicitly.");
  }
  if (src_size_bytes > dst_entry->size_bytes - dst_offset_bytes) {
    util::panic(
        "Specified size_bytes exceeds the available size in either the source or destination buffer from the "
        "respective offset");
  }

  void* final_src_data = static_cast<char*>(src_data) + src_offset_bytes;

  // == Copy via mapping ==
  if (dst_entry->mapping != nullptr) {
    // Use the exisiting mapping
    std::memcpy(static_cast<char*>(dst_entry->mapping) + dst_offset_bytes, final_src_data, src_size_bytes);
    return;
  } else if (dst_entry->mappable) {
    // Try to map the memory
    void* mapping = nullptr;
    auto result   = vmaMapMemory(allocator_, dst_entry->allocation, &mapping);
    if (result == VK_SUCCESS) {
      std::memcpy(static_cast<char*>(mapping) + dst_offset_bytes, final_src_data, src_size_bytes);
      vmaUnmapMemory(allocator_, dst_entry->allocation);
      return;
    }
  }

  // == Copy via staging buffer ==
  auto staging_buffer = create_temp_staging_buffer(src_size_bytes);
  auto result = vmaCopyMemoryToAllocation(allocator_, final_src_data, staging_buffer.allocation, 0, src_size_bytes);
  if (result != VK_SUCCESS) {
    util::panic("Failed to copy data to staging buffer");
  }
  VkBufferCopy cpy_info = {
      .srcOffset = 0,
      .dstOffset = dst_offset_bytes,
      .size      = src_size_bytes,
  };
  device_->immediate_command_submit([&staging_buffer, &dst_entry, &cpy_info](VkCommandBuffer cmd_buff) {
    vkCmdCopyBuffer(cmd_buff, staging_buffer.buffer, dst_entry->buffer, 1, &cpy_info);
  });
  vmaDestroyBuffer(allocator_, staging_buffer.buffer, staging_buffer.allocation);
}

void ResourceManager::buffer_cpy(BufferHandle dst, BufferHandle src, VkDeviceSize size_bytes,
                                 VkDeviceSize dst_offset_bytes, VkDeviceSize src_offset_bytes) const {
  const auto* dst_entry = buffer_pool_.get_entry(dst);
  const auto* src_entry = buffer_pool_.get_entry(src);

  // == Bounds checking ==
  if (dst_entry == nullptr) {
    util::panic("Invalid buffer handle");
  }
  if (src_entry == nullptr) {
    util::panic("Invalid buffer handle");
  }
  if (src_offset_bytes >= src_entry->size_bytes) {
    util::panic("src_offset_bytes exceeds or equals the source buffer size");
  }
  if (dst_offset_bytes >= dst_entry->size_bytes) {
    util::panic("dst_offset_bytes exceeds or equals the source buffer size");
  }
  if (size_bytes == VK_WHOLE_SIZE &&
      src_entry->size_bytes - src_offset_bytes != dst_entry->size_bytes - dst_offset_bytes) {
    util::panic(
        "When size_bytes is VK_WHOLE_SIZE, the remaining size in both buffers (from the respective offsets) must be "
        "equal");
  }
  if (size_bytes != VK_WHOLE_SIZE && (size_bytes > src_entry->size_bytes - src_offset_bytes ||
                                      size_bytes > dst_entry->size_bytes - dst_offset_bytes)) {
    util::panic(
        "Specified size_bytes exceeds the available size in either the source or destination buffer from the "
        "respective offsets");
  }

  // == Perform copy ==
  auto dst_buffer = dst_entry->buffer;
  auto src_buffer = src_entry->buffer;

  VkBufferCopy cpy_info = {
      .srcOffset = src_offset_bytes,
      .dstOffset = dst_offset_bytes,
      .size      = size_bytes,
  };
  if (size_bytes == VK_WHOLE_SIZE) {
    cpy_info.size =
        src_entry->size_bytes - src_offset_bytes;  // == dst_entry->size_bytes - dst_offset_bytes, as checked above
  }
  device_->immediate_command_submit([&src_buffer, &dst_buffer, &cpy_info](VkCommandBuffer cmd_buff) {
    vkCmdCopyBuffer(cmd_buff, src_buffer, dst_buffer, 1, &cpy_info);
  });
}

Result<void*, Error> ResourceManager::buffer_map(BufferHandle buffer) {
  auto* entry = buffer_pool_.get_entry(buffer);
  if (entry->mapping != nullptr) {
    util::Logger::warn("Requested mapping to a buffer that is already mapped!");
    return entry->mapping;
  }

  void* mapping;
  auto result = vmaMapMemory(allocator_, entry->allocation, &mapping);
  if (result != VK_SUCCESS) {
    util::Logger::err("Could map memory with VMA");
    return std::unexpected(Error{
        .msg     = "VMA mapping failed",
        .code    = ErrorCode::MemoryMappingFailure{},
        .vk_code = vk::Result(result),
    });
  }

  entry->mapping = mapping;
  return mapping;
}

void ResourceManager::buffer_unmap(BufferHandle buffer) {
  auto* entry = buffer_pool_.get_entry(buffer);
  if (entry->mapping == nullptr) {
    util::Logger::warn("Buffer is not mapped, but unmap has been requested!");
    return;
  }
  vmaUnmapMemory(allocator_, entry->allocation);
  entry->mapping = nullptr;
}

Result<void*, Error> ResourceManager::get_buffer_mapping(BufferHandle buffer) const {
  auto* entry = buffer_pool_.get_entry(buffer);
  return entry->mapping != nullptr ? std::expected<void*, Error>(entry->mapping)
                                   : std::unexpected(Error{
                                         .msg  = "Buffer is not mapped, so there is no valid mapping to return",
                                         .code = ErrorCode::MemoryMappingNotSupported{},
                                     });
}

VkDeviceSize ResourceManager::get_buffer_size_bytes(BufferHandle buffer) const {
  const auto* entry = buffer_pool_.get_entry(buffer);
  return entry->size_bytes;
}

VkBuffer ResourceManager::get_buffer_vk(BufferHandle buffer) const {
  const auto* entry = buffer_pool_.get_entry(buffer);
  return entry->buffer;
}

VkDescriptorBufferInfo ResourceManager::get_buffer_descriptor_info(BufferHandle buffer, VkDeviceSize offset,
                                                                   VkDeviceSize range) const {
  return VkDescriptorBufferInfo{
      .buffer = get_buffer_vk(buffer),
      .offset = offset,
      .range  = range,
  };
}

void ResourceManager::destroy_buffer(BufferHandle buffer) {
  const auto* entry = buffer_pool_.get_entry(buffer);
  vmaDestroyBuffer(allocator_, entry->buffer, entry->allocation);
  buffer_pool_.delete_entry(buffer);
}

BufferResourceEntry ResourceManager::create_temp_staging_buffer(VkDeviceSize size_bytes) const {
  auto buff_create_info = VkBufferCreateInfo{
      .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size        = size_bytes,
      .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = VMA_MEMORY_USAGE_AUTO;
  alloc_create_info.flags                   = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  VmaAllocationInfo alloc_info;
  VkBuffer buffer     = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;
  VkResult res = vmaCreateBuffer(allocator_, &buff_create_info, &alloc_create_info, &buffer, &alloc, &alloc_info);

  if (res != VK_SUCCESS) {
    util::panic("Failed to create staging buffer");
  }

  return BufferResourceEntry{
      .buffer     = buffer,
      .allocation = alloc,
      .size_bytes = size_bytes,
      .mapping    = nullptr,
      .usage      = buff_create_info.usage,
      .mappable   = true,
  };
}

}  // namespace eray::vkren
