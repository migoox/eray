#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/memory_region.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/image_format_helpers.hpp>
#include <liberay/vkren/vk_util.hpp>
#include <liberay/vkren/vma_raii_object.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

Result<ImageResource, Error> ImageResource::create_attachment_image(const Device& device, ImageDescription desc,
                                                                    vk::ImageUsageFlags usage,
                                                                    vk::ImageAspectFlags aspect,
                                                                    vk::SampleCountFlagBits sample_count) {
  auto image_info = vk::ImageCreateInfo{
      .sType       = vk::StructureType::eImageCreateInfo,
      .imageType   = desc.image_type(),
      .format      = desc.format,
      .extent      = vk::Extent3D{.width = desc.width, .height = desc.height, .depth = desc.depth},
      .mipLevels   = 1,
      .arrayLayers = 1,
      .samples     = sample_count,
      .tiling      = vk::ImageTiling::eOptimal,
      .usage       = usage,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  auto alloc_create_info     = VmaAllocationCreateInfo{};
  alloc_create_info.usage    = VMA_MEMORY_USAGE_AUTO;
  alloc_create_info.flags    = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
  alloc_create_info.priority = 1.0F;

  VkImage vkimg{};
  VmaAllocation alloc{};
  VmaAllocationInfo alloc_info;
  auto result = vmaCreateImage(device.allocator(), reinterpret_cast<VkImageCreateInfo*>(&image_info),
                               &alloc_create_info, &vkimg, &alloc, &alloc_info);
  if (result != VK_SUCCESS) {
    return std::unexpected(Error{
        .msg     = "Failed to create staging buffer",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = vk::Result(result),
    });
  }

  return ImageResource{
      ._image      = VMARaiiImage(device.allocator(), alloc, vkimg),
      .description = desc,
      ._p_device   = &device,
      .mip_levels  = 1,
      .aspect      = aspect,
      .usage       = usage,
  };
}

Result<ImageResource, Error> ImageResource::create_texture(const Device& device, ImageDescription desc, bool mipmapping,
                                                           vk::ImageAspectFlags aspect) {
  assert(!helper::is_block_format(desc.format) && "Block Compression Formats are not supported yet!");

  const auto usage =
      vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;

  auto image_info = vk::ImageCreateInfo{
      .sType       = vk::StructureType::eImageCreateInfo,
      .imageType   = desc.image_type(),
      .format      = desc.format,
      .extent      = vk::Extent3D{.width = desc.width, .height = desc.height, .depth = desc.depth},
      .mipLevels   = mipmapping ? desc.find_mip_levels() : 1,
      .arrayLayers = desc.array_layers,
      .samples     = vk::SampleCountFlagBits::e1,
      .tiling      = vk::ImageTiling::eOptimal,
      .usage       = usage,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  auto alloc_create_info     = VmaAllocationCreateInfo{};
  alloc_create_info.usage    = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  alloc_create_info.priority = 1.0F;

  VkImage vkimg{};
  VmaAllocation alloc{};
  VmaAllocationInfo alloc_info;
  auto result = vmaCreateImage(device.allocator(), reinterpret_cast<VkImageCreateInfo*>(&image_info),
                               &alloc_create_info, &vkimg, &alloc, &alloc_info);
  if (result != VK_SUCCESS) {
    return std::unexpected(Error{
        .msg     = "Failed to create staging buffer",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = vk::Result(result),
    });
  }

  return ImageResource{
      ._image      = VMARaiiImage(device.allocator(), alloc, vkimg),
      .description = std::move(desc),
      ._p_device   = &device,
      .mip_levels  = mipmapping ? desc.find_mip_levels() : 1,
      .aspect      = aspect,
      .usage       = usage,
  };
}

Result<void, Error> ImageResource::upload(util::MemoryRegion src_region) {
  const auto full_size = find_full_size_bytes();
  assert((mipmapping_enabled() && src_region.size_bytes() == full_size) ||
         src_region.size_bytes() == lod0_size_bytes() &&
             "Expected either LOD=0 image level or full image with all of the mipmap levels");
  assert((usage & vk::ImageUsageFlagBits::eTransferDst) && "Image is not a transfer destination, upload impossible");

  // == Copy data from the staging buffer to the image layers ==========================================================
  {
    const auto copy_mip_levels = (mipmapping_enabled() && src_region.size_bytes() == full_size) ? mip_levels : 1;

    auto staging_buffer = BufferResource::create_staging_buffer(*_p_device, src_region);
    if (!staging_buffer) {
      util::Logger::err("Could not upload a texture. Staging buffer creation failed!");
      return std::unexpected(staging_buffer.error());
    }

    auto cmd_cpy_buff = _p_device->begin_single_time_commands();
    auto mip_width    = description.width;
    auto mip_height   = description.height;
    auto mip_depth    = description.depth;
    auto mip_offset   = 0U;
    for (auto mip_level = 0U; mip_level < copy_mip_levels; ++mip_level) {
      const auto mip_size_bytes =
          helper::bytes_per_pixel(description.format) * mip_width * mip_height * mip_depth * description.array_layers;

      for (auto layer = 0U; layer < description.array_layers; ++layer) {
        const auto copy_region = vk::BufferImageCopy{
            .bufferOffset = mip_offset,

            // No padding bytes between rows of the image is assumed
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,

            .imageSubresource =
                vk::ImageSubresourceLayers{
                    .aspectMask     = aspect,
                    .mipLevel       = mip_level,
                    .baseArrayLayer = 0,
                    .layerCount     = description.array_layers,
                },
            .imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
            .imageExtent = description.extent(),
        };

        transition_layout(cmd_cpy_buff, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        cmd_cpy_buff.copyBufferToImage(staging_buffer->_buffer._handle, _image._handle,
                                       vk::ImageLayout::eTransferDstOptimal, copy_region);
      }
      mip_offset += mip_size_bytes;
      mip_width  = std::max(mip_width / 2U, 1U);
      mip_height = std::max(mip_height / 2U, 1U);
      mip_depth  = std::max(mip_depth / 2U, 1U);
    }
    _p_device->end_single_time_commands(cmd_cpy_buff);
  }

  // Return if mipmapping is disabled or the memory region already contained pregenerated mipmaps.
  if (!mipmapping_enabled() || src_region.size_bytes() == full_size) {
    return {};
  }

  // == Generate mipmaps using linear blitting =========================================================================

  // Check if linear blitting is supported
  auto format_props = _p_device->physical_device().getFormatProperties(description.format);
  if (!(format_props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
    util::Logger::err(
        "Texture creation failed due to lack of support for linear blitting. Mipmaps could not be generated");
    return std::unexpected(Error{
        .msg  = "Mipmapping impossible, because linear blitting is not supported for the specified format",
        .code = ErrorCode::PhysicalDeviceNotSufficient{},
    });
  }

  auto cmd_buff = _p_device->begin_single_time_commands();

  auto mip_width  = static_cast<int32_t>(description.width);
  auto mip_height = static_cast<int32_t>(description.height);
  auto mip_depth  = static_cast<int32_t>(description.depth);
  for (uint32_t i = 1; i < mip_levels; ++i) {
    transition_mip_level_layout(cmd_buff, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
                                i - 1);

    auto offsets     = vk::ArrayWrapper1D<vk::Offset3D, 2>();
    auto dst_offsets = vk::ArrayWrapper1D<vk::Offset3D, 2>();

    offsets[0] = vk::Offset3D(0, 0, 0);
    offsets[1] = vk::Offset3D(mip_width, mip_height, mip_depth);

    dst_offsets[0] = vk::Offset3D(0, 0, 0);
    dst_offsets[1] = vk::Offset3D(mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1,
                                  mip_depth > 1 ? mip_depth / 2 : 1);

    auto blit = vk::ImageBlit{
        .srcSubresource =
            vk::ImageSubresourceLayers{
                .aspectMask     = aspect,
                .mipLevel       = i - 1,
                .baseArrayLayer = 0,
                .layerCount     = description.array_layers,
            },
        .srcOffsets = offsets,
        .dstSubresource =
            vk::ImageSubresourceLayers{
                .aspectMask     = aspect,
                .mipLevel       = i,
                .baseArrayLayer = 0,
                .layerCount     = description.array_layers,
            },
        .dstOffsets = dst_offsets,
    };

    cmd_buff.blitImage(image(), vk::ImageLayout::eTransferSrcOptimal, image(), vk::ImageLayout::eTransferDstOptimal,
                       {blit}, vk::Filter::eLinear);

    transition_mip_level_layout(cmd_buff, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                                i - 1);

    if (mip_width > 1) {
      mip_width /= 2;
    }
    if (mip_height > 1) {
      mip_height /= 2;
    }
    if (mip_depth > 1) {
      mip_depth /= 2;
    }
  }

  transition_mip_level_layout(cmd_buff, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                              mip_levels - 1);

  _p_device->end_single_time_commands(cmd_buff);

  return {};
}

Result<vk::raii::ImageView, Error> ImageResource::create_image_view(vk::ImageViewType image_view_type) const {
  auto img_create_info = vk::ImageViewCreateInfo{
      .image    = image(),
      .viewType = image_view_type,
      .format   = description.format,
      .components =
          vk::ComponentMapping{
              .r = vk::ComponentSwizzle::eIdentity,
              .g = vk::ComponentSwizzle::eIdentity,
              .b = vk::ComponentSwizzle::eIdentity,
              .a = vk::ComponentSwizzle::eIdentity,
          },
      .subresourceRange =
          vk::ImageSubresourceRange{
              .aspectMask     = aspect,
              .baseMipLevel   = 0,
              .levelCount     = mip_levels,
              .baseArrayLayer = 0,
              .layerCount     = description.array_layers,
          },
  };

  auto img_view_opt = (*_p_device)->createImageView(img_create_info);
  if (!img_view_opt) {
    util::Logger::err("Could not create an image view: {}", vk::to_string(img_view_opt.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan Image View creation failed",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = img_view_opt.error(),
    });
  }

  return std::move(*img_view_opt);
}

Result<vk::raii::ImageView, Error> ImageResource::create_image_view() const {
  if (description.image_type() == vk::ImageType::e2D) {
    return create_image_view(vk::ImageViewType::e2D);
  }
  return create_image_view(vk::ImageViewType::e3D);
}

void ImageResource::transition_layout(vk::CommandBuffer cmd, vk::ImageLayout current_layout,
                                      vk::ImageLayout new_layout) {
  vk_util::transition_image_barrier(cmd, image(),
                                    vk::ImageSubresourceRange{
                                        .aspectMask     = aspect,
                                        .baseMipLevel   = 0,
                                        .levelCount     = mip_levels,
                                        .baseArrayLayer = 0,
                                        .layerCount     = description.array_layers,
                                    },
                                    current_layout, new_layout);
}

void ImageResource::transition_mip_level_layout(vk::CommandBuffer cmd, vk::ImageLayout current_layout,
                                                vk::ImageLayout new_layout, uint32_t base_level, uint32_t level_count) {
  vk_util::transition_image_barrier(cmd, image(),
                                    vk::ImageSubresourceRange{
                                        .aspectMask     = aspect,
                                        .baseMipLevel   = base_level,
                                        .levelCount     = level_count,
                                        .baseArrayLayer = 0,
                                        .layerCount     = description.array_layers,
                                    },
                                    current_layout, new_layout);
}

}  // namespace eray::vkren
