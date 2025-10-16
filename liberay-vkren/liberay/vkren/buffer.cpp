#include <vulkan/vulkan_core.h>

#include <cassert>
#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/vma_allocation_manager.hpp>
#include <liberay/vkren/vma_raii_object.hpp>
#include <optional>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
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

  return ExclusiveBufferResource(std::move(*buffer_opt), std::move(*buffer_mem_opt), info.size_bytes, info.buff_usage,
                                 info.mem_properties, &device);
}
Result<ExclusiveBufferResource, Error> ExclusiveBufferResource::create_staging(const Device& device,
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

  auto staging_buffer =
      vkren::ExclusiveBufferResource::create_staging(device, src_region).or_panic("Staging buffer creation failed");
  auto new_info = info;
  new_info.buff_usage |= vk::BufferUsageFlagBits::eTransferDst;
  auto result = vkren::ExclusiveBufferResource::create(device, new_info);
  if (!result) {
    return std::unexpected(result.error());
  }
  result->copy_from(staging_buffer.buffer_, vk::BufferCopy(0, 0, info.size_bytes));
  return std::move(*result);
}

void ExclusiveBufferResource::fill_data(util::MemoryRegion src_region, vk::DeviceSize offset_bytes) const {
  void* dst = memory_.mapMemory(offset_bytes, src_region.size_bytes());
  memcpy(dst, src_region.data(), src_region.size_bytes());
  memory_.unmapMemory();

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
  if (!has_flag(usage_, vk::BufferUsageFlagBits::eTransferDst)) {
    util::panic("Assertion failed! Buffer must be a transfer destination (VK_BUFFER_USAGE_TRANSFER_DST_BIT)");
  }

  auto cmd_cpy_buff = p_device_->begin_single_time_commands();
  cmd_cpy_buff.copyBuffer(src_buff, buffer_, cpy_info);
  p_device_->end_single_time_commands(cmd_cpy_buff);
}

Result<BufferResource, Error> BufferResource::create_staging_buffer(Device& device,
                                                                    const util::MemoryRegion& src_region) {
  auto buf_create_info = vk::BufferCreateInfo{
      .sType = vk::StructureType::eBufferCreateInfo,
      .size  = src_region.size_bytes(),
      .usage = vk::BufferUsageFlagBits::eTransferSrc,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = VMA_MEMORY_USAGE_AUTO;
  alloc_create_info.flags                   = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  auto buff_opt = device.vma_alloc_manager().create_buffer(buf_create_info, alloc_create_info);
  if (!buff_opt) {
    std::unexpected(buff_opt.error());
  }

  vmaCopyMemoryToAllocation(device.vma_alloc_manager().allocator(), src_region.data(), buff_opt->allocation, 0,
                            src_region.size_bytes());

  return BufferResource{
      ._buffer             = VmaRaiiBuffer(device.vma_alloc_manager(), buff_opt->allocation, buff_opt->vk_buffer),
      ._p_device           = &device,
      .size_bytes          = src_region.size_bytes(),
      .usage               = buf_create_info.usage,
      .transfer_src        = true,
      .persistently_mapped = false,
      .mappable            = true,
  };
}

Result<PersistentlyMappedBufferResource, Error> BufferResource::persistently_mapped_staging_buffer(
    Device& device, vk::DeviceSize size_bytes) {
  auto buf_create_info = vk::BufferCreateInfo{
      .sType = vk::StructureType::eBufferCreateInfo,
      .size  = size_bytes,
      .usage = vk::BufferUsageFlagBits::eTransferSrc,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = VMA_MEMORY_USAGE_AUTO;
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo alloc_info;
  auto buff_opt = device.vma_alloc_manager().create_buffer(buf_create_info, alloc_create_info, alloc_info);
  if (!buff_opt) {
    std::unexpected(buff_opt.error());
  }

  if (!alloc_info.pMappedData) {
    return std::unexpected(Error{
        .msg  = "Persistent mapping failed: allocation did not provide pMappedData",
        .code = ErrorCode::VulkanObjectCreationFailure{},
    });
  }

  return PersistentlyMappedBufferResource{
      .buffer =
          BufferResource{
              ._buffer      = VmaRaiiBuffer(device.vma_alloc_manager(), buff_opt->allocation, buff_opt->vk_buffer),
              ._p_device    = &device,
              .size_bytes   = size_bytes,
              .usage        = buf_create_info.usage,
              .transfer_src = true,
              .persistently_mapped = true,
              .mappable            = true,
          },
      .mapped_data = alloc_info.pMappedData,
  };
}

Result<PersistentlyMappedBufferResource, Error> BufferResource::create_readback_storage_buffer(
    Device& device, vk::DeviceSize size_bytes) {
  return create_readback_buffer(device, size_bytes, vk::BufferUsageFlagBits::eStorageBuffer);
}

Result<PersistentlyMappedBufferResource, Error> BufferResource::create_readback_buffer(
    Device& device, vk::DeviceSize size_bytes, vk::BufferUsageFlags additional_usage_flags) {
  auto buf_create_info = vk::BufferCreateInfo{
      .sType = vk::StructureType::eBufferCreateInfo,
      .size  = size_bytes,
      .usage = vk::BufferUsageFlagBits::eTransferDst | additional_usage_flags,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = VMA_MEMORY_USAGE_AUTO;
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo alloc_info;
  auto buff_opt = device.vma_alloc_manager().create_buffer(buf_create_info, alloc_create_info, alloc_info);
  if (!buff_opt) {
    std::unexpected(buff_opt.error());
  }

  if (!alloc_info.pMappedData) {
    return std::unexpected(Error{
        .msg  = "Persistent mapping failed: allocation did not provide pMappedData",
        .code = ErrorCode::VulkanObjectCreationFailure{},
    });
  }

  return PersistentlyMappedBufferResource{
      .buffer =
          BufferResource{
              ._buffer      = VmaRaiiBuffer(device.vma_alloc_manager(), buff_opt->allocation, buff_opt->vk_buffer),
              ._p_device    = &device,
              .size_bytes   = size_bytes,
              .usage        = buf_create_info.usage,
              .transfer_src = false,
              .persistently_mapped = true,
              .mappable            = true,
          },
      .mapped_data = alloc_info.pMappedData,
  };
}

Result<BufferResource, Error> BufferResource::create_gpu_local_buffer(Device& device, vk::DeviceSize size_bytes,
                                                                      vk::BufferUsageFlags usage) {
  auto buf_create_info = vk::BufferCreateInfo{
      .sType       = vk::StructureType::eBufferCreateInfo,
      .size        = size_bytes,
      .usage       = usage | vk::BufferUsageFlagBits::eTransferDst,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  auto buff_opt = device.vma_alloc_manager().create_buffer(buf_create_info, alloc_create_info);
  if (!buff_opt) {
    std::unexpected(buff_opt.error());
  }

  return BufferResource{
      ._buffer             = VmaRaiiBuffer(device.vma_alloc_manager(), buff_opt->allocation, buff_opt->vk_buffer),
      ._p_device           = &device,
      .size_bytes          = size_bytes,
      .usage               = buf_create_info.usage,
      .transfer_src        = false,
      .persistently_mapped = false,
      .mappable            = false,
  };
}

Result<void, Error> BufferResource::write_via_staging_buffer(const util::MemoryRegion& src_region,
                                                             vk::DeviceSize offset) const {
  assert(offset < size_bytes && "Offset exceeds the buffer size");
  assert(src_region.size_bytes() <= size_bytes - offset && "Region size exceeds available space in the buffer");

  auto staging_buff = create_staging_buffer(*_p_device, src_region);
  if (!staging_buff) {
    return std::unexpected(staging_buff.error());
  }

  auto cmd_cpy_buff = _p_device->begin_single_time_commands();
  cmd_cpy_buff.copyBuffer(staging_buff->_buffer._vk_handle, _buffer._vk_handle,
                          vk::BufferCopy(0, offset, src_region.size_bytes()));
  _p_device->end_single_time_commands(cmd_cpy_buff);

  return {};
}

Result<BufferResource, Error> BufferResource::create_uniform_buffer(Device& device, vk::DeviceSize size_bytes) {
  vk::BufferCreateInfo buf_create_info = {
      .sType       = vk::StructureType::eBufferCreateInfo,
      .size        = size_bytes,
      .usage       = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = VMA_MEMORY_USAGE_AUTO;
  alloc_create_info.flags                   = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                            VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo alloc_info;
  auto buff_opt = device.vma_alloc_manager().create_buffer(buf_create_info, alloc_create_info, alloc_info);
  if (!buff_opt) {
    std::unexpected(buff_opt.error());
  }

  VkMemoryPropertyFlags mem_prop_flags = 0;
  vmaGetAllocationMemoryProperties(device.vma_alloc_manager().allocator(), buff_opt->allocation, &mem_prop_flags);
  bool mappable_flag = false;
  if (mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    mappable_flag = true;
  }

  return BufferResource{
      ._buffer             = VmaRaiiBuffer(device.vma_alloc_manager(), buff_opt->allocation, buff_opt->vk_buffer),
      ._p_device           = &device,
      .size_bytes          = size_bytes,
      .usage               = buf_create_info.usage,
      .transfer_src        = false,
      .persistently_mapped = alloc_info.pMappedData != nullptr,
      .mappable            = mappable_flag,
  };
}

Result<BufferResource, Error> BufferResource::create_storage_buffer(Device& device, vk::DeviceSize size_bytes) {
  vk::BufferCreateInfo buf_create_info = {
      .sType       = vk::StructureType::eBufferCreateInfo,
      .size        = size_bytes,
      .usage       = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VmaAllocationInfo alloc_info;
  auto buff_opt = device.vma_alloc_manager().create_buffer(buf_create_info, alloc_create_info, alloc_info);
  if (!buff_opt) {
    std::unexpected(buff_opt.error());
  }

  return BufferResource{
      ._buffer             = VmaRaiiBuffer(device.vma_alloc_manager(), buff_opt->allocation, buff_opt->vk_buffer),
      ._p_device           = &device,
      .size_bytes          = size_bytes,
      .usage               = buf_create_info.usage,
      .transfer_src        = false,
      .persistently_mapped = false,
      .mappable            = false,
  };
}

[[nodiscard]] Result<PersistentlyMappedBufferResource, Error> BufferResource::create_persistently_mapped_uniform_buffer(
    Device& device, vk::DeviceSize size_bytes) {
  // TODO(migoox): Read about HOST_COHERENT & HOST_CACHED (might improve the performance)
  vk::BufferCreateInfo buf_create_info = {
      .sType       = vk::StructureType::eBufferCreateInfo,
      .size        = size_bytes,
      .usage       = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  VmaAllocationCreateInfo alloc_create_info = {};
  alloc_create_info.usage                   = VMA_MEMORY_USAGE_AUTO;
  alloc_create_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

  VmaAllocationInfo alloc_info;
  auto buff_opt = device.vma_alloc_manager().create_buffer(buf_create_info, alloc_create_info, alloc_info);
  if (!buff_opt) {
    std::unexpected(buff_opt.error());
  }

  if (!alloc_info.pMappedData) {
    return std::unexpected(Error{
        .msg  = "Persistent mapping failed: allocation did not provide pMappedData",
        .code = ErrorCode::VulkanObjectCreationFailure{},
    });
  }

  return PersistentlyMappedBufferResource{
      .buffer =
          BufferResource{
              ._buffer      = VmaRaiiBuffer(device.vma_alloc_manager(), buff_opt->allocation, buff_opt->vk_buffer),
              ._p_device    = &device,
              .size_bytes   = size_bytes,
              .usage        = buf_create_info.usage,
              .transfer_src = false,
              .persistently_mapped = true,
              .mappable            = true,
          },
      .mapped_data = alloc_info.pMappedData,
  };
}

Result<void, Error> BufferResource::write(const util::MemoryRegion& src_region, vk::DeviceSize offset) const {
  assert(offset < size_bytes && "Offset exceeds the buffer size");
  assert(src_region.size_bytes() <= size_bytes - offset && "Region size exceeds available space in the buffer");

  if (mappable) {
    auto res = vmaCopyMemoryToAllocation(_p_device->vma_alloc_manager().allocator(), src_region.data(),
                                         _buffer._allocation, offset, src_region.size_bytes());

    if (res != VK_SUCCESS) {
      return std::unexpected(Error{
          .msg     = "Failed to fill buffer",
          .code    = ErrorCode::VulkanObjectCreationFailure{},
          .vk_code = vk::Result(res),
      });
    }
  } else {
    return write_via_staging_buffer(src_region, offset);
  }

  return {};
}

Result<void*, Error> BufferResource::map() const {
  if (!mappable) {
    util::Logger::err("Buffer is not mappable, but map has been requested!");
    return std::unexpected(Error{
        .msg  = "Buffer is not mappable",
        .code = ErrorCode::MemoryMappingNotSupported{},
    });
  }

  auto alloc_info = this->alloc_info();
  // If it’s already persistently mapped (created with VMA_ALLOCATION_CREATE_MAPPED_BIT),
  // VMA provides a stable pointer here.
  if (alloc_info.pMappedData) {
    return alloc_info.pMappedData;
  }

  void* ptr    = nullptr;
  VkResult res = vmaMapMemory(_p_device->vma_alloc_manager().allocator(), _buffer._allocation, &ptr);
  if (res != VK_SUCCESS) {
    util::Logger::err("Could map memory with VMA");
    return std::unexpected(Error{
        .msg     = "VMA mapping failed",
        .code    = ErrorCode::MemoryMappingFailure{},
        .vk_code = vk::Result(res),
    });
  }

  return ptr;
}

void BufferResource::write(const BufferResource& buffer, vk::DeviceSize offset) const {
  auto cmd_cpy_buff = _p_device->begin_single_time_commands();
  cmd_cpy_buff.copyBuffer(buffer._buffer._vk_handle, _buffer._vk_handle, vk::BufferCopy(0, offset, buffer.size_bytes));
  _p_device->end_single_time_commands(cmd_cpy_buff);
}

void BufferResource::unmap() const {
  auto alloc_info = this->alloc_info();
  // If it’s already persistently mapped (created with VMA_ALLOCATION_CREATE_MAPPED_BIT),
  // VMA provides a stable pointer here.
  if (alloc_info.pMappedData) {
    return;
  }

  vmaUnmapMemory(_p_device->vma_alloc_manager().allocator(), _buffer._allocation);
}

std::optional<void*> BufferResource::mapping() const {
  auto info = this->alloc_info();
  if (info.pMappedData == nullptr) {
    return std::nullopt;
  }

  return info.pMappedData;
}

}  // namespace eray::vkren
