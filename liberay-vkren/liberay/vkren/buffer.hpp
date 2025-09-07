#pragma once

#include <vma/vk_mem_alloc.h>

#include <liberay/util/memory_region.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/vma_raii_object.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {
/**
 * @brief Represents a buffer with dedicated chunk of device memory. The buffer resource owns both -- buffer object
 * and the chunk of memory.
 *
 * @note This buffer abstraction is trivial and according to https://developer.nvidia.com/vulkan-memory-management,
 * should not be used frequently. Moreover since each buffer has it's own DeviceMemory in this scheme, there can be
 * up to 4096 ExclusiveBufferResources in your app.
 *
 * @warning Lifetime is bound by the device lifetime.
 *
 */
class ExclusiveBufferResource {
 public:
  ExclusiveBufferResource() = delete;
  explicit ExclusiveBufferResource(std::nullptr_t) {}

  struct CreateInfo {
    vk::DeviceSize size_bytes;
    vk::BufferUsageFlags buff_usage;
    vk::MemoryPropertyFlags mem_properties =
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
  };

  [[nodiscard]] static Result<ExclusiveBufferResource, Error> create(const Device& device, const CreateInfo& info);

  /**
   * @brief Creates a buffer and uploads provided `src_data` to it via temporary staging buffer.
   * VK_BUFFER_USAGE_TRANSFER_DST_BIT is automatically appended to the info.buff_usage.
   *
   * @return Result<ExclusiveBufferResource, Error>
   */
  [[nodiscard]] static Result<ExclusiveBufferResource, Error> create_and_upload_via_staging_buffer(
      const Device& device, const CreateInfo& info, util::MemoryRegion src_region);

  [[nodiscard]] static Result<ExclusiveBufferResource, Error> create_staging(const Device& device,
                                                                             util::MemoryRegion src_region);

  /**
   * @brief Copies CPU `src_data` to GPU memory. Creates vk::SharingMode::eExclusive buffer. Uses map to achieve
   * this.
   *
   * @warning This call might require flushing the if VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is not set.
   *
   * @param src_region
   * @param offset_bytes
   *
   */
  void fill_data(util::MemoryRegion src_region, vk::DeviceSize offset_bytes = 0) const;

  /**
   * @brief Blocks the program execution and copies GPU `src_buff` data to the other GPU buffer.
   *
   * @warning Requires the `src_buff` to be a VK_BUFFER_USAGE_TRANSFER_SRC_BIT set and the current buffer to
   * have VK_BUFFER_USAGE_TRANSFER_DST_BIT set.
   *
   * @param src_buff
   * @param cpy_info
   */
  void copy_from(const vk::raii::Buffer& src_buff, vk::BufferCopy cpy_info) const;

  const vk::raii::Buffer& buffer() const { return buffer_; }
  const vk::raii::DeviceMemory& memory() const { return memory_; }
  vk::DeviceSize memory_size_bytes() const { return mem_size_bytes_; }
  vk::BufferUsageFlags usage() const { return usage_; }
  vk::MemoryPropertyFlags memory_properties() const { return mem_properties_; }
  const Device& device() const { return *p_device_; }

 private:
  ExclusiveBufferResource(vk::raii::Buffer&& buffer, vk::raii::DeviceMemory&& memory, vk::DeviceSize mem_size_bytes,
                          vk::BufferUsageFlags usage, vk::MemoryPropertyFlags mem_properties,
                          observer_ptr<const Device> p_device)
      : buffer_(std::move(buffer)),
        memory_(std::move(memory)),
        p_device_(p_device),
        mem_size_bytes_(mem_size_bytes),
        usage_(usage),
        mem_properties_(mem_properties) {}

  vk::raii::Buffer buffer_             = nullptr;
  vk::raii::DeviceMemory memory_       = nullptr;
  observer_ptr<const Device> p_device_ = nullptr;
  vk::DeviceSize mem_size_bytes_{};
  vk::BufferUsageFlags usage_;
  vk::MemoryPropertyFlags mem_properties_;
};

struct PersistentlyMappedBufferResource;

/**
 * @brief Buffer allocated with VMA.
 *
 */
struct BufferResource {
  VmaRaiiBuffer _buffer = VmaRaiiBuffer(nullptr);
  observer_ptr<Device> _p_device;
  vk::DeviceSize size_bytes;
  vk::BufferUsageFlags usage;
  bool transfer_src;
  bool persistently_mapped;
  bool mappable;

  /**
   * @brief A "staging" buffer than you want to map and fill from CPU code, then use as a source of transfer to some GPU
   * resource.
   *
   * @param device
   * @param size_bytes
   * @return Result<Buffer, Error>
   */
  [[nodiscard]] static Result<BufferResource, Error> create_staging_buffer(Device& device,
                                                                           const util::MemoryRegion& src_region);

  /**
   * @brief Creates a gpu local buffer, e.g. for vertex or index buffer.
   *
   * @param device
   * @param size_bytes
   * @param usage
   * @return Result<Buffer, Error>
   */
  [[nodiscard]] static Result<BufferResource, Error> create_gpu_local_buffer(Device& device, vk::DeviceSize size_bytes,
                                                                             vk::BufferUsageFlagBits usage);

  [[nodiscard]] static Result<BufferResource, Error> create_index_buffer(Device& device, vk::DeviceSize size_bytes) {
    return create_gpu_local_buffer(device, size_bytes, vk::BufferUsageFlagBits::eIndexBuffer);
  }
  [[nodiscard]] static Result<BufferResource, Error> create_vertex_buffer(Device& device, vk::DeviceSize size_bytes) {
    return create_gpu_local_buffer(device, size_bytes, vk::BufferUsageFlagBits::eVertexBuffer);
  }

  /**
   * @brief Buffers for data written by or transferred from the GPU that you want to read back on the CPU, e.g. results
   * of some computations.
   *
   * @param device
   * @param size_bytes
   * @return Result<Buffer, Error>
   */
  [[nodiscard]] static Result<PersistentlyMappedBufferResource, Error> create_readback_buffer(
      Device& device, vk::DeviceSize size_bytes);

  /**
   * @brief For resources that you frequently write on CPU via mapped pointer and frequently read on GPU e.g. uniform
   * buffer (also called "dynamic"). This buffer might not be mappable, to upload data safely use `write()`.
   *
   * @param device
   * @param size_bytes
   * @return Result<Buffer, Error>
   */
  [[nodiscard]] static Result<BufferResource, Error> create_uniform_buffer(Device& device, vk::DeviceSize size_bytes);

  [[nodiscard]] static Result<PersistentlyMappedBufferResource, Error> create_persistently_mapped_uniform_buffer(
      Device& device, vk::DeviceSize size_bytes);

  /**
   * @brief Creates a temporary staging buffer and uses it to fill the buffer.
   * This function blocks the CPU until the write is ready.
   *
   * @param offset Represents the destination (GPU memory) offset. Zero by default.
   * @param src_region
   * @return Result<void, Error>
   */
  Result<void, Error> write_via_staging_buffer(const util::MemoryRegion& src_region, vk::DeviceSize offset = 0) const;

  /**
   * @brief Fills the buffer. If the buffer is not `mappable` it will call `fill_via_staging_buffer()`.
   * This function blocks the CPU until the write is ready.
   *
   * @param offset Represents the destination (GPU memory) offset. Zero by default.
   * @param src_region
   * @return Result<void, Error>
   */
  Result<void, Error> write(const util::MemoryRegion& src_region, vk::DeviceSize offset = 0) const;

  /**
   * @brief Maps the buffer on demand. If the buffer is persistently mapped (VMA_ALLOCATION_CREATE_MAPPED_BIT) the
   * function will just return the pointer to the mapping.
   *
   * @warning The buffer must be `mappable`, the assertion will fail otherwise.
   *
   * @return Result<void, Error>
   */
  Result<void*, Error> map() const;

  /**
   * @brief This function has no effect if the buffer is persistently mapped (VMA_ALLOCATION_CREATE_MAPPED_BIT).
   *
   */
  void unmap() const;

  /**
   * @brief Returns VMA allocation info for the current buffer.
   *
   * @warning The lifetime of this info is bounded by the lifetime of the Buffer.
   *
   * @return VmaAllocationInfo
   */
  VmaAllocationInfo alloc_info() const { return _buffer.alloc_info(); }

  /**
   * @brief If buffer is mapped this function returns valid void* ptr to the mapping and std::nullopt
   * otherwise. If buffer is persistently mapped, this function returns valid void* ptr.
   *
   * @return std::optional<void*>
   */
  std::optional<void*> mapping() const;

  vk::Buffer buffer() const { return _buffer._vk_handle; }
};

/**
 * @brief Represents a GPU buffer that is mapped to CPU through it's entire lifetime.
 *
 */
struct PersistentlyMappedBufferResource {
  BufferResource buffer;
  void* mapped_data;
};

}  // namespace eray::vkren
