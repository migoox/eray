#pragma once

#include <vulkan/vulkan_core.h>

#include <liberay/res/image.hpp>
#include <liberay/util/memory_region.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/vma_raii_object.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

struct ImageResource {
  VMARaiiImage _image;
  ImageDescription description;
  observer_ptr<const Device> _p_device;
  bool mipmapping;

  /**
   * @brief Any resources that you frequently write and read on GPU, e.g. images used as color attachments (aka "render
   * targets"), depth-stencil attachments, images/buffers used as storage image/buffer (aka "Unordered Access View
   * (UAV)"). The buffer has always usage set to eSampled.
   *
   * @param device
   * @param desc
   * @param sample_count
   * @return Result<Image, Error>
   */
  [[nodiscard]] static Result<ImageResource, Error> create_attachment_image(
      const Device& device, ImageDescription desc, vk::ImageUsageFlags usage,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1);

  [[nodiscard]] static Result<ImageResource, Error> create_color_attachment_image(
      const Device& device, const ImageDescription& desc,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return create_attachment_image(
        device, desc, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
        sample_count);
  }

  [[nodiscard]] static Result<ImageResource, Error> create_depth_stencil_attachment_image(
      const Device& device, const ImageDescription& desc,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return create_attachment_image(device, desc, vk::ImageUsageFlagBits::eDepthStencilAttachment, sample_count);
  }

  /**
   * @brief Use this for buffers that are frequently sampled by the GPU, and loaded once from the CPU.
   *
   * @param device
   * @param desc
   * @return Result<ImageResource, Error>
   */
  [[nodiscard]] static Result<ImageResource, Error> create_texture(const Device& device, ImageDescription desc,
                                                                   bool mipmapping = true);

  /**
   * @brief Automatically detects whether the buffer contains all mipmaps. If so it uploads the buffer to the texture.
   * If the buffer contains LOD0 only and `mipmapping` is set to true, this function uploads the LOD0 image(s) and
   * generates the missing mipmaps.
   *
   * @param src_region Represents packed region of CPU memory that consists of mip levels. A mip level with LOD `i`
   * contains all layer images with LOD `i`.
   * @param offset
   * @return Result<void, Error>
   */

  Result<void, Error> upload(util::MemoryRegion src_region,
                             vk::ImageAspectFlags aspect_mask = vk::ImageAspectFlagBits::eColor) const;

  VmaAllocationInfo alloc_info() const { return _image.alloc_info(); }

  vk::Image image() const { return _image._handle; }

  /**
   * @brief Size of the image in level of detail 0. The function ignores the mipmap level and layers.
   *
   * @return vk::DeviceSize
   */
  vk::DeviceSize lod0_size_bytes() const { return description.lod0_size_bytes(); }

  /**
   * @brief Full size in bytes, includes mipmaps and layers.
   *
   * @return vk::DeviceSize
   */
  vk::DeviceSize find_full_size_bytes() const { return description.find_full_size_bytes(); }
};

}  // namespace eray::vkren
