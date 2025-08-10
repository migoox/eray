#include <liberay/vkren/image.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace eray::vkren {
Result<ExclusiveImage2DResource, ExclusiveImage2DResource::CreationError> ExclusiveImage2DResource::create(
    const Device& device, const CreateInfo& info) {
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
    util::Logger::err("Could not create a buffer object: {}", vk::to_string(image_opt.error()));
    return std::unexpected(image_opt.error());
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
    return std::unexpected(mem_type_opt.error());
  }

  auto alloc_info = vk::MemoryAllocateInfo{
      .allocationSize  = mem_requirements.size,
      .memoryTypeIndex = *mem_type_opt,
  };
  auto image_mem_opt = device->allocateMemory(alloc_info);
  if (!image_mem_opt) {
    util::Logger::err("Could not allocate memory for a buffer object: {}", vk::to_string(image_mem_opt.error()));
    return std::unexpected(image_mem_opt.error());
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

}  // namespace eray::vkren
