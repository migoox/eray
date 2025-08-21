#include <expected>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

Result<ExclusiveImage2DResource, Error> ExclusiveImage2DResource::create_texture_image_from_mipmaps(
    const Device& device, ImageDescription desc, const void* data, vk::DeviceSize size_in_bytes) {
  // Staging buffer
  auto staging_buffer = vkren::ExclusiveBufferResource::create(  //
      device,
      vkren::ExclusiveBufferResource::CreateInfo{
          .size_in_bytes  = size_in_bytes,
          .buff_usage     = vk::BufferUsageFlagBits::eTransferSrc,
          .mem_properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      });
  if (!staging_buffer) {
    return std::unexpected(staging_buffer.error());
  }

  staging_buffer->fill_data(data, 0, size_in_bytes);

  // Image object
  auto txt_image = vkren::ExclusiveImage2DResource::create(
      device, vkren::ExclusiveImage2DResource::CreateInfo{
                  .size_in_bytes = size_in_bytes,

                  // We want to sample the image in the fragment shader
                  // transfer src for mipmap generation
                  .image_usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled |
                                 vk::ImageUsageFlagBits::eTransferSrc,

                  .desc = desc,

                  // Texels are laid out in an implementation defined order for optimal access
                  .tiling = vk::ImageTiling::eOptimal,

                  .mem_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
              });
  if (!txt_image) {
    return std::unexpected(txt_image.error());
  }

  // Copy mip maps data
  device.transition_image_layout(txt_image->image, txt_image->desc, vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eTransferDstOptimal);
  auto cmd_buff    = device.begin_single_time_commands();
  auto copy_region = vk::BufferImageCopy{
      .bufferOffset = 0,

      // For example, you could have some padding bytes between rows of the image. Specifying 0 for both indicates that
      // the pixels are simply tightly packed like they are in our case.
      .bufferRowLength   = 0,
      .bufferImageHeight = 0,

      // The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy
      // the pixels.

      .imageSubresource =
          vk::ImageSubresourceLayers{
              .aspectMask     = vk::ImageAspectFlagBits::eColor,
              .mipLevel       = 0,
              .baseArrayLayer = 0,
              .layerCount     = 1,
          },
      .imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
      .imageExtent =
          vk::Extent3D{
              .width  = desc.width,
              .height = desc.height,
              .depth  = 1,
          },
  };

  auto mip_width  = static_cast<uint32_t>(desc.width);
  auto mip_height = static_cast<uint32_t>(desc.height);
  for (uint32_t i = 0; i < desc.mip_levels; ++i) {
    copy_region.imageSubresource.mipLevel = i;
    copy_region.imageExtent               = vk::Extent3D{.width = mip_width, .height = mip_height, .depth = 1};
    cmd_buff.copyBufferToImage(staging_buffer->buffer, txt_image->image, vk::ImageLayout::eTransferDstOptimal,
                               copy_region);

    copy_region.bufferOffset += mip_width * mip_height * 4;
    mip_width  = std::max(mip_width / 2U, 1U);
    mip_height = std::max(mip_height / 2U, 1U);
  }
  device.end_single_time_commands(cmd_buff);
  device.transition_image_layout(txt_image->image, txt_image->desc, vk::ImageLayout::eTransferDstOptimal,
                                 vk::ImageLayout::eShaderReadOnlyOptimal);

  return txt_image;
}

Result<ExclusiveImage2DResource, Error> ExclusiveImage2DResource::create_texture_image(const Device& device,
                                                                                       ImageDescription desc,
                                                                                       const void* data,
                                                                                       vk::DeviceSize size_in_bytes) {
  // Staging buffer
  auto staging_buffer = vkren::ExclusiveBufferResource::create(  //
      device,
      vkren::ExclusiveBufferResource::CreateInfo{
          .size_in_bytes  = size_in_bytes,
          .buff_usage     = vk::BufferUsageFlagBits::eTransferSrc,
          .mem_properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      });
  if (!staging_buffer) {
    return std::unexpected(staging_buffer.error());
  }

  staging_buffer->fill_data(data, 0, size_in_bytes);

  // Image object
  auto txt_image = vkren::ExclusiveImage2DResource::create(
      device, vkren::ExclusiveImage2DResource::CreateInfo{
                  .size_in_bytes = size_in_bytes,

                  // We want to sample the image in the fragment shader
                  // transfer src for mipmap generation
                  .image_usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled |
                                 vk::ImageUsageFlagBits::eTransferSrc,

                  .desc = desc,

                  // Texels are laid out in an implementation defined order for optimal access
                  .tiling = vk::ImageTiling::eOptimal,

                  .mem_properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
              });
  if (!txt_image) {
    return std::unexpected(txt_image.error());
  }

  device.transition_image_layout(txt_image->image, txt_image->desc, vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eTransferDstOptimal);
  txt_image->copy_from(device, staging_buffer->buffer);
  if (desc.mip_levels == 1) {
    device.transition_image_layout(txt_image->image, txt_image->desc, vk::ImageLayout::eTransferDstOptimal,
                                   vk::ImageLayout::eShaderReadOnlyOptimal);
  } else {
    device.generate_mipmaps(txt_image->image, txt_image->desc);
  }

  return txt_image;
}

Result<ExclusiveImage2DResource, Error> ExclusiveImage2DResource::create(const Device& device, const CreateInfo& info) {
  // == Create Image Object ===========================================================================================

  auto image_info = vk::ImageCreateInfo{
      .imageType   = vk::ImageType::e2D,
      .format      = info.desc.format,
      .extent      = vk::Extent3D{.width = info.desc.width, .height = info.desc.height, .depth = 1},
      .mipLevels   = info.desc.mip_levels,
      .arrayLayers = 1,
      .samples     = vk::SampleCountFlagBits::e1,
      .tiling      = info.tiling,
      .usage       = info.image_usage,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  auto image_opt = device->createImage(image_info);
  if (!image_opt) {
    util::Logger::err("Could not create an image object: {}", vk::to_string(image_opt.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan Image Creation failed",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = image_opt.error(),
    });
  }

  // == Allocate Device Memory =========================================================================================
  //
  // The first step of allocating memory for the buffer is to query its memory requirements
  // - size: describes the size required memory in bytes may differ from buffer_info.size
  // - alignment: the offset in bytes where the buffer begins in the allocated region of memory, depends on usage and
  //              flags
  // - memoryTypeBits: Bit field of the memory types that are suitable for the buffer
  //
  auto mem_requirements = image_opt->getMemoryRequirements();
  auto mem_type_opt     = device.find_mem_type(mem_requirements.memoryTypeBits, info.mem_properties);
  if (!mem_type_opt) {
    util::Logger::err("Could not find a memory type that meets the buffer memory requirements");
    return std::unexpected(Error{
        .msg     = "No memory type that meets the buffer memory requirements",
        .code    = ErrorCode::NoSuitableMemoryTypeFailure{},
        .vk_code = vk::Result::eSuccess,
    });
  }

  auto alloc_info = vk::MemoryAllocateInfo{
      .allocationSize  = mem_requirements.size,
      .memoryTypeIndex = *mem_type_opt,
  };
  auto image_mem_opt = device->allocateMemory(alloc_info);
  if (!image_mem_opt) {
    util::Logger::err("Could not allocate memory for a buffer object: {}", vk::to_string(image_mem_opt.error()));
    return std::unexpected(Error{
        .msg     = "Vulkan memory allocation failed",
        .code    = ErrorCode::MemoryAllocationFailure{},
        .vk_code = image_mem_opt.error(),
    });
  }
  image_opt->bindMemory(*image_mem_opt, 0);

  return ExclusiveImage2DResource{
      .image             = std::move(*image_opt),
      .memory            = std::move(*image_mem_opt),
      .mem_size_in_bytes = mem_requirements.size,
      .image_usage       = info.image_usage,
      .mem_properties    = info.mem_properties,
      .desc              = info.desc,
  };
}

void ExclusiveImage2DResource::copy_from(const Device& device, const vk::raii::Buffer& src_buff) const {
  auto cmd_buff    = device.begin_single_time_commands();
  auto copy_region = vk::BufferImageCopy{
      .bufferOffset = 0,

      // For example, you could have some padding bytes between rows of the image. Specifying 0 for both indicates that
      // the pixels are simply tightly packed like they are in our case.
      .bufferRowLength   = 0,
      .bufferImageHeight = 0,

      // The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy
      // the pixels.

      .imageSubresource =
          vk::ImageSubresourceLayers{
              .aspectMask     = vk::ImageAspectFlagBits::eColor,
              .mipLevel       = 0,
              .baseArrayLayer = 0,
              .layerCount     = 1,
          },
      .imageOffset = vk::Offset3D{.x = 0, .y = 0, .z = 0},
      .imageExtent =
          vk::Extent3D{
              .width  = desc.width,
              .height = desc.height,
              .depth  = 1,
          },
  };
  cmd_buff.copyBufferToImage(src_buff, image, vk::ImageLayout::eTransferDstOptimal, copy_region);
  device.end_single_time_commands(cmd_buff);
}

Result<vk::raii::ImageView, Error> ExclusiveImage2DResource::create_image_view(const Device& device,
                                                                               vk::ImageAspectFlags aspect_mask) {
  auto img_create_info = vk::ImageViewCreateInfo{
      .image    = image,
      .viewType = vk::ImageViewType::e2D,
      .format   = desc.format,
      .components =
          vk::ComponentMapping{
              .r = vk::ComponentSwizzle::eIdentity,
              .g = vk::ComponentSwizzle::eIdentity,
              .b = vk::ComponentSwizzle::eIdentity,
              .a = vk::ComponentSwizzle::eIdentity,
          },
      .subresourceRange =
          vk::ImageSubresourceRange{
              .aspectMask     = aspect_mask,
              .baseMipLevel   = 0,
              .levelCount     = desc.mip_levels,
              .baseArrayLayer = 0,
              .layerCount     = 1  //
          },
  };

  auto img_view_opt = device->createImageView(img_create_info);
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

}  // namespace eray::vkren
