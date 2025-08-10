#pragma once

#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <variant>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

/**
 * @brief Represents an image with dedicated chunk of device memory. The buffer resource owns both -- buffer object and
 * the chunk of memory.
 *
 * @note This buffer abstraction is trivial and according to https://developer.nvidia.com/vulkan-memory-management,
 * should not be used frequently. Moreover since each buffer has it's own DeviceMemory in this scheme, there can be
 * up to 4096 ExclusiveImageResources in your app.
 *
 */
struct ExclusiveImage2DResource {
  vk::raii::Image image         = nullptr;
  vk::raii::DeviceMemory memory = nullptr;
  vk::DeviceSize mem_size_in_bytes;
  vk::ImageUsageFlags image_usage;
  vk::Format format;
  uint32_t width;
  uint32_t height;
  vk::MemoryPropertyFlags mem_properties;

  struct CreateInfo {
    vk::DeviceSize size_in_bytes;
    vk::ImageUsageFlags image_usage;
    vk::Format format;
    uint32_t width;
    uint32_t height;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    vk::MemoryPropertyFlags mem_properties;
  };

  using CreationError = std::variant<vk::Result, Device::NoSuitableMemoryTypeError>;
  static Result<ExclusiveImage2DResource, CreationError> create(const Device& device, const CreateInfo& info);

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
  void copy_from(const Device& device, const vk::raii::CommandPool& cmd_pool, const vk::raii::Buffer& src_buff,
                 vk::BufferCopy cpy_info) const;
};
}  // namespace eray::vkren
