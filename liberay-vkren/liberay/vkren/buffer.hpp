#pragma once

#include <liberay/util/memory_region.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
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

  [[nodiscard]] static Result<ExclusiveBufferResource, Error> create_staging_buffer(const Device& device,
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

}  // namespace eray::vkren
