#include <liberay/vkren/image.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {
Result<ExclusiveImage2DResource, Error> ExclusiveImage2DResource::create(const Device& device, const CreateInfo& info) {
  // == Create Image Object ===========================================================================================

  auto image_info = vk::ImageCreateInfo{
      .imageType   = vk::ImageType::e2D,
      .format      = info.format,
      .extent      = vk::Extent3D{.width = info.width, .height = info.height, .depth = 1},
      .mipLevels   = 1,
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
      .format            = info.format,
      .width             = info.width,
      .height            = info.height,
      .mem_properties    = info.mem_properties,
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
              .width  = width,
              .height = height,
              .depth  = 1,
          },
  };
  cmd_buff.copyBufferToImage(src_buff, image, vk::ImageLayout::eTransferDstOptimal, copy_region);
  device.end_single_time_commands(cmd_buff);
}

Result<vk::raii::ImageView, Error> ExclusiveImage2DResource::create_img_view(const Device& device,
                                                                             vk::ImageAspectFlags aspect_mask) {
  auto img_create_info = vk::ImageViewCreateInfo{
      .image    = image,
      .viewType = vk::ImageViewType::e2D,
      .format   = format,
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
              .levelCount     = 1,
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
