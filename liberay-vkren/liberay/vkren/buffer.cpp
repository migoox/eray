#include <liberay/util/logger.hpp>
#include <liberay/vkren/buffer.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace eray::vkren {

Result<BufferResource, BufferResource::CreationError> BufferResource::create_exclusive(
    const Device& device, vk::DeviceSize size_in_bytes, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties) {
  // == Create Buffer Object ===========================================================================================

  auto buffer_info = vk::BufferCreateInfo{
      .size        = size_in_bytes,
      .usage       = usage,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  auto buffer_opt = device->createBuffer(buffer_info);
  if (!buffer_opt) {
    util::Logger::err("Could not create a buffer object: {}", vk::to_string(buffer_opt.error()));
    return std::unexpected(buffer_opt.error());
  }

  // == Allocate Device Memory =========================================================================================
  //
  // The first step of allocating memory for the buffer is to query its memory requirements
  // - size: describes the size required memory in bytes may differ from buffer_info.size
  // - alignment: the offset in bytes where the buffer begins in the allocated region of memory, depends on usage and
  //              flags
  // - memoryTypeBits: Bit field of the memory types that are suitable for the buffer
  //
  auto mem_requirements = buffer_opt->getMemoryRequirements();
  auto mem_type_opt     = device.find_mem_type(mem_requirements.memoryTypeBits, properties);
  if (!mem_type_opt) {
    util::Logger::err("Could not find a memory type that meets the buffer memory requirements");
    return std::unexpected(mem_type_opt.error());
  }

  auto alloc_info = vk::MemoryAllocateInfo{
      .allocationSize  = mem_requirements.size,
      .memoryTypeIndex = *mem_type_opt,
  };
  auto buffer_mem_opt = device->allocateMemory(alloc_info);
  if (!buffer_mem_opt) {
    util::Logger::err("Could not allocate memory for a buffer object: {}", vk::to_string(buffer_mem_opt.error()));
    return std::unexpected(buffer_mem_opt.error());
  }
  buffer_opt->bindMemory(*buffer_mem_opt, 0);

  return BufferResource{
      .buffer            = std::move(*buffer_opt),
      .memory            = std::move(*buffer_mem_opt),
      .mem_size_in_bytes = size_in_bytes,
      .usage             = usage,
      .properties        = properties,
  };
}

void BufferResource::fill_data(const void* src_data, vk::DeviceSize offset_in_bytes,
                               vk::DeviceSize size_in_bytes) const {
  void* dst = memory.mapMemory(offset_in_bytes, size_in_bytes);
  memcpy(dst, src_data, size_in_bytes);
  memory.unmapMemory();

  // Unfortunately, the driver may not immediately copy the data into the buffer memory, for example, because of
  // caching. It is also possible that writes to the buffer are not visible in the mapped memory yet. That's why
  // we used VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, otherwise we would have to call flush after writing and call
  // invalidate before reading.
  //
  // Note: Explicit flushing might increase performance in some cases.

  // The transfer of data to the GPU is an operation that happens in the background, and the specification simply
  // tells us that it is guaranteed to be complete as of the next call to vkQueueSubmit
}

}  // namespace eray::vkren
