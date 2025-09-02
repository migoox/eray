#pragma once

#include <cstddef>
#include <liberay/res/image.hpp>
#include <liberay/util/memory_region.hpp>
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
 * @warning Lifetime is bound by the device lifetime.
 *
 */
class ExclusiveImage2DResource {
 public:
  ExclusiveImage2DResource() = delete;
  explicit ExclusiveImage2DResource(std::nullptr_t) {}

  struct CreateInfo {
    vk::DeviceSize size_bytes;
    vk::ImageUsageFlags image_usage;
    ImageDescription desc;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    vk::MemoryPropertyFlags mem_properties;
    vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1;
  };

  [[nodiscard]] static Result<ExclusiveImage2DResource, Error> create(const Device& device, const CreateInfo& info);

  /**
   * @brief If desc.mip_levels > 1 than the mipmaps will be generated with Vulkan Blitting.
   *
   * @param device
   * @param desc
   * @param data
   * @param size_bytes
   * @return Result<ExclusiveImage2DResource, Error>
   */
  [[nodiscard]] static Result<ExclusiveImage2DResource, Error> create_texture(const Device& device,
                                                                              ImageDescription desc,
                                                                              util::MemoryRegion region);

  /**
   * @brief Creates texture from CPU image representation. Generates mipmaps with Vulkan Blitting. If Vulkan Blitting is
   * not supported uses CPU algorithm fallback.
   *
   * @param device
   * @param desc
   * @param generate_mipmaps
   * @return Result<ExclusiveImage2DResource, Error>
   */
  [[nodiscard]] static Result<ExclusiveImage2DResource, Error> create_texture(const Device& device,
                                                                              const res::Image& image,
                                                                              bool generate_mipmaps = true);

  /**
   * @brief Expects a buffer containing packed images with LOD ranging 0 to mip levels - 1.
   *
   * @param device
   * @param desc
   * @param mipmaps_buffer
   * @param size_bytes
   * @return Result<ExclusiveImage2DResource, Error>
   */
  [[nodiscard]] static Result<ExclusiveImage2DResource, Error> create_texture_from_mipmaps_buffer(
      const Device& device, ImageDescription desc, util::MemoryRegion mipmaps_region);

  /**
   * @brief Blocks the program execution and copies GPU `src_buff` data to the other GPU buffer.
   *
   * @warning Requires the `src_buff` to be a VK_BUFFER_USAGE_TRANSFER_SRC_BIT set and the current buffer to
   * have VK_BUFFER_USAGE_TRANSFER_DST_BIT set.
   *
   * @param src_buff
   */
  void copy_from(const vk::raii::Buffer& src_buff) const;

  Result<vk::raii::ImageView, Error> create_image_view(
      vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor);

  const ImageDescription& desc() const { return desc_; }
  const vk::raii::Image& image() const { return image_; }
  const vk::raii::DeviceMemory& memory() const { return memory_; }
  const Device& device() const { return *p_device_; }
  vk::DeviceSize mem_size_bytes() const { return mem_size_bytes_; }
  vk::ImageUsageFlags image_usage() const { return image_usage_; }
  vk::MemoryPropertyFlags mem_properties() const { return mem_properties_; }

 private:
  static Result<void, Error> copy_mip_maps_data(const Device& device, const ExclusiveImage2DResource& image,
                                                util::MemoryRegion mipmaps_region);

 private:
  ExclusiveImage2DResource(ImageDescription&& desc, vk::raii::Image&& image, vk::raii::DeviceMemory&& memory,
                           const Device* p_device, vk::DeviceSize mem_size_bytes, vk::ImageUsageFlags image_usage,
                           vk::MemoryPropertyFlags mem_properties)
      : desc_(std::move(desc)),
        image_(std::move(image)),
        memory_(std::move(memory)),
        p_device_(p_device),
        mem_size_bytes_(mem_size_bytes),
        image_usage_(image_usage),
        mem_properties_(mem_properties) {}

  ImageDescription desc_{};
  vk::raii::Image image_               = nullptr;
  vk::raii::DeviceMemory memory_       = nullptr;
  observer_ptr<const Device> p_device_ = nullptr;
  vk::DeviceSize mem_size_bytes_{};
  vk::ImageUsageFlags image_usage_;
  vk::MemoryPropertyFlags mem_properties_;
};

}  // namespace eray::vkren
