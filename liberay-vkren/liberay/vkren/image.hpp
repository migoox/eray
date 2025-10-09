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
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

struct ImageResource {
  VmaRaiiImage _image = VmaRaiiImage(nullptr);
  ImageDescription description;
  observer_ptr<Device> _p_device = nullptr;
  uint32_t mip_levels;
  vk::ImageAspectFlags aspect;
  vk::ImageUsageFlags usage;

  /**
   * @brief Any resources that you frequently write and read on GPU, e.g. images used as color attachments (aka "render
   * targets"), depth-stencil attachments, images/buffers used as storage image/buffer (aka "Unordered Access View
   * (UAV)"). The buffer has always usage set to eSampled.
   *
   * The layout is VK_IMAGE_LAYOUT_UNDEFINED.
   *
   * @param device
   * @param desc
   * @param sample_count
   * @return Result<Image, Error>
   */
  [[nodiscard]] static Result<ImageResource, Error> create_attachment_image(
      Device& device, ImageDescription desc, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1);

  [[nodiscard]] static Result<ImageResource, Error> create_color_attachment_image(
      Device& device, const ImageDescription& desc,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return create_attachment_image(
        device, desc, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor, sample_count);
  }

  [[nodiscard]] static Result<ImageResource, Error> create_depth_stencil_attachment_image(
      Device& device, const ImageDescription& desc,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return create_attachment_image(device, desc, vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                   vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, sample_count);
  }

  /**
   * @brief Use this for buffers that are frequently sampled by the GPU, and loaded once from the CPU. The layout is
   * VK_IMAGE_LAYOUT_UNDEFINED.
   *
   * @param device
   * @param desc
   * @return Result<ImageResource, Error>
   */
  [[nodiscard]] static Result<ImageResource, Error> create_texture(
      Device& device, ImageDescription desc, bool mipmapping = true,
      vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

  /**
   * @brief Invokes `vkCmdPipelineBarrier2`. `cmd` must be in the begin state.
   *
   * @param cmd
   * @param new_layout
   */
  void transition_layout(vk::CommandBuffer cmd, vk::ImageLayout current_layout, vk::ImageLayout new_layout);

  void transition_mip_level_layout(vk::CommandBuffer cmd, vk::ImageLayout current_layout, vk::ImageLayout new_layout,
                                   uint32_t base_level, uint32_t level_count = 1);

  /**
   * @brief Automatically detects whether the buffer contains all mipmaps. If so it uploads the buffer to the texture.
   * If the buffer contains LOD0 only and `mipmapping` is set to true, this function uploads the LOD0 image(s) and
   * generates the missing mipmaps.
   *
   * Expects the layout of the image range to be VK_IMAGE_LAYOUT_UNDEFINED. Leaves the layout in the
   * VK_IMAGE_SHADER_READ_ONLY_OPTIMAL state.
   *
   * @param src_region Represents packed region of CPU memory that consists of mip levels. A mip level with LOD `i`
   * contains all layer images with LOD `i`.
   * @param offset
   * @return Result<void, Error>
   */
  Result<void, Error> upload(util::MemoryRegion src_region);

  VmaAllocationInfo alloc_info() const { return _image.alloc_info(); }

  vk::Image vk_image() const { return _image._vk_handle; }

  Result<vk::raii::ImageView, Error> create_image_view(vk::ImageViewType image_view_type) const;

  /**
   * @brief In case of `vk::ImageType::e2D` returns image view with `vk::ImageViewType::e2D`. Returns image view
   * `vk::ImageViewType::e3D` otherwise.
   *
   * @return Result<vk::raii::ImageView, Error>
   */
  Result<vk::raii::ImageView, Error> create_image_view() const;

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

  /**
   * @brief Returns true iff the image resource has mipmappnig enabled.
   *
   * @return true
   * @return false
   */
  bool mipmapping_enabled() const { return mip_levels > 1; }

  vk::ImageSubresourceRange full_resource_range() const;
};

}  // namespace eray::vkren
