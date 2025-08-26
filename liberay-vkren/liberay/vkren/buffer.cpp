#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/error.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

Result<ExclusiveBufferResource, Error> ExclusiveBufferResource::create(const Device& device, const CreateInfo& info) {
  // == Create Buffer Object ===========================================================================================

  auto buffer_info = vk::BufferCreateInfo{
      .size        = info.size_bytes,
      .usage       = info.buff_usage,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  auto buffer_opt = device->createBuffer(buffer_info);
  if (!buffer_opt) {
    util::Logger::err("Could not create a buffer object: {}", vk::to_string(buffer_opt.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan Buffer Object creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = buffer_opt.error(),
    });
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
  auto mem_type_opt     = device.find_mem_type(mem_requirements.memoryTypeBits, info.mem_properties);
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
    return std::unexpected(Error{
        .msg     = "Could not allocate memory",
        .code    = ErrorCode::MemoryAllocationFailure{},
        .vk_code = buffer_mem_opt.error(),
    });
  }
  buffer_opt->bindMemory(*buffer_mem_opt, 0);

  return ExclusiveBufferResource{
      .buffer         = std::move(*buffer_opt),
      .memory         = std::move(*buffer_mem_opt),
      .mem_size_bytes = info.size_bytes,
      .usage          = info.buff_usage,
      .mem_properties = info.mem_properties,
      .p_device       = &device,
  };
}
Result<ExclusiveBufferResource, Error> ExclusiveBufferResource::create_staging_buffer(const Device& device,
                                                                                      util::MemoryRegion src_region) {
  auto staging_buff_opt =
      vkren::ExclusiveBufferResource::create(device, vkren::ExclusiveBufferResource::CreateInfo{
                                                         .size_bytes = src_region.size_bytes(),
                                                         .buff_usage = vk::BufferUsageFlagBits::eTransferSrc,
                                                     });
  if (!staging_buff_opt) {
    util::Logger::err("Could not create a staging buffer");
    return std::unexpected(staging_buff_opt.error());
  }
  staging_buff_opt->fill_data(src_region, 0);
  return std::move(*staging_buff_opt);
}

Result<ExclusiveBufferResource, Error> ExclusiveBufferResource::create_and_upload_via_staging_buffer(
    const Device& device, const CreateInfo& info, util::MemoryRegion src_region) {
  if (src_region.size_bytes() != info.size_bytes) {
    util::panic("Region size and creation info size mismatch");
  }

  auto staging_buffer = vkren::ExclusiveBufferResource::create_staging_buffer(device, src_region)
                            .or_panic("Staging buffer creation failed");
  auto new_info = info;
  new_info.buff_usage |= vk::BufferUsageFlagBits::eTransferDst;
  auto result = vkren::ExclusiveBufferResource::create(device, new_info);
  if (!result) {
    return std::unexpected(result.error());
  }
  result->copy_from(staging_buffer.buffer, vk::BufferCopy(0, 0, info.size_bytes));
  return std::move(*result);
}

void ExclusiveBufferResource::fill_data(util::MemoryRegion src_region, vk::DeviceSize offset_bytes) const {
  void* dst = memory.mapMemory(offset_bytes, src_region.size_bytes());
  memcpy(dst, src_region.data(), src_region.size_bytes());
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

void ExclusiveBufferResource::copy_from(const vk::raii::Buffer& src_buff, vk::BufferCopy cpy_info) const {
  if (!has_flag(usage, vk::BufferUsageFlagBits::eTransferDst)) {
    util::panic("Assertion failed! Buffer must be a transfer destination (VK_BUFFER_USAGE_TRANSFER_DST_BIT)");
  }

  auto cmd_cpy_buff = p_device->begin_single_time_commands();
  cmd_cpy_buff.copyBuffer(src_buff, buffer, cpy_info);
  p_device->end_single_time_commands(cmd_cpy_buff);
}

}  // namespace eray::vkren
