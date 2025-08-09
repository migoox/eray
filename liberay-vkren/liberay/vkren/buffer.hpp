#pragma once

#include <liberay/util/result.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/result.hpp>
#include <variant>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

/**
 * @brief Represents a buffer with dedicated chunk of device memory. The buffer resource owns both -- buffer object and
 * the chunk of memory.
 *
 */
struct BufferResource {
  vk::raii::Buffer buffer       = nullptr;
  vk::raii::DeviceMemory memory = nullptr;
  vk::DeviceSize size_in_bytes;
  vk::BufferUsageFlags usage;
  vk::MemoryPropertyFlags properties;

  using CreationError = std::variant<vk::Result, Device::NoSuitableMemoryTypeError>;
  static Result<BufferResource, CreationError> create_exclusive(
      const Device& device, vk::DeviceSize size_in_bytes, vk::BufferUsageFlags usage,
      vk::MemoryPropertyFlags properties = vk::MemoryPropertyFlagBits::eHostVisible |
                                           vk::MemoryPropertyFlagBits::eHostCoherent);

  /**
   * @brief Copies `src_data` to GPU memory.
   *
   * @warning This call might require flushing the if VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is not set.
   *
   * @param src_data
   * @param offset_in_bytes
   * @param size_in_bytes
   */
  void fill_data(const void* src_data, vk::DeviceSize offset_in_bytes, vk::DeviceSize size_in_bytes) const;
};

}  // namespace eray::vkren
