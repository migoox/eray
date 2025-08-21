#pragma once

#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/image_description.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

/**
 * @brief Represents an image with dedicated chunk of device memory. The buffer resource owns both -- buffer object and
 * the chunk of memory.
 *
 * @note This buffer abstraction is trivial and according to https://developer.nvidia.com/vulkan-memory-management,
 * should not be used frequently. Moreover since each image has it's own DeviceMemory in this scheme, there can be
 * up to 4096 ExclusiveImageResources in your app.
 *
 */
struct ExclusiveImage2DResource {
  vk::raii::Image image         = nullptr;
  vk::raii::DeviceMemory memory = nullptr;
  vk::DeviceSize mem_size_in_bytes;
  vk::ImageUsageFlags image_usage;
  vk::MemoryPropertyFlags mem_properties;
  ImageDescription desc;

  struct CreateInfo {
    vk::DeviceSize size_in_bytes;
    vk::ImageUsageFlags image_usage;
    ImageDescription desc;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    vk::MemoryPropertyFlags mem_properties;
  };

  static Result<ExclusiveImage2DResource, Error> create(const Device& device, const CreateInfo& info);
  static Result<ExclusiveImage2DResource, Error> create_texture_image(const Device& device, ImageDescription desc,
                                                                      const void* data, vk::DeviceSize size_in_bytes,
                                                                      bool generate_mipmaps = false);

  /**
   * @brief Blocks the program execution and copies GPU `src_buff` data to the other GPU buffer.
   *
   * @warning Requires the `src_buff` to be a VK_BUFFER_USAGE_TRANSFER_SRC_BIT set and the current buffer to
   * have VK_BUFFER_USAGE_TRANSFER_DST_BIT set.
   *
   * @param src_buff
   */
  void copy_from(const Device& device, const vk::raii::Buffer& src_buff) const;

  Result<vk::raii::ImageView, Error> create_image_view(
      const Device& device, vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor);
};

}  // namespace eray::vkren
