#pragma once

#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

/**
 * @brief Represents a buffer with dedicated chunk of device memory. The buffer resource owns both -- buffer object and
 * the chunk of memory.
 *
 * @note This buffer abstraction is trivial and according to https://developer.nvidia.com/vulkan-memory-management,
 * should not be used frequently. Moreover since each buffer has it's own DeviceMemory in this scheme, there can be
 * up to 4096 ExclusiveBufferResources in your app.
 *
 * @warning Lifetime is bound by the device lifetime.
 *
 */
struct ExclusiveBufferResource {
  vk::raii::Buffer buffer       = nullptr;
  vk::raii::DeviceMemory memory = nullptr;
  vk::DeviceSize mem_size_in_bytes;
  vk::BufferUsageFlags usage;
  vk::MemoryPropertyFlags mem_properties;
  observer_ptr<const Device> p_device = nullptr;

  struct CreateInfo {
    vk::DeviceSize size_in_bytes;
    vk::BufferUsageFlags buff_usage;
    vk::MemoryPropertyFlags mem_properties =
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
  };

  [[nodiscard]] static Result<ExclusiveBufferResource, Error> create(const Device& device, const CreateInfo& info);

  /**
   * @brief Creates a buffer and uploads provided `src_data` to it via temporary staging buffer.
   * VK_BUFFER_USAGE_TRANSFER_DST_BIT is automatically appended to the info.buff_usage.
   *
   * @param device
   * @param info
   * @param src_data
   * @return Result<ExclusiveBufferResource, Error>
   */
  [[nodiscard]] static Result<ExclusiveBufferResource, Error> create_and_upload_via_staging_buffer(
      const Device& device, const CreateInfo& info, const void* src_data);

  [[nodiscard]] static Result<ExclusiveBufferResource, Error> create_staging_buffer(const Device& device,
                                                                                    const void* src_data,
                                                                                    vk::DeviceSize size_in_bytes);

  /**
   * @brief Copies CPU `src_data` to GPU memory. Creates vk::SharingMode::eExclusive buffer. Uses map to achieve
   * this.
   *
   * @warning This call might require flushing the if VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is not set.
   *
   * @param src_data
   * @param offset_in_bytes
   * @param size_in_bytes
   */
  void fill_data(const void* src_data, vk::DeviceSize offset_in_bytes, vk::DeviceSize size_in_bytes) const;

  /**
   * @brief Blocks the program execution and copies GPU `src_buff` data to the other GPU buffer.
   *
   * @warning Requires the `src_buff` to be a VK_BUFFER_USAGE_TRANSFER_SRC_BIT set and the current buffer to
   * have VK_BUFFER_USAGE_TRANSFER_DST_BIT set.
   *
   * @param src_buff
   */
  void copy_from(const vk::raii::Buffer& src_buff, vk::BufferCopy cpy_info) const;
};

}  // namespace eray::vkren
