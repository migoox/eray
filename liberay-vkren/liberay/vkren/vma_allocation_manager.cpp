#include <liberay/util/variant_match.hpp>
#include <liberay/vkren/vma_allocation_manager.hpp>

namespace eray::vkren {

VmaAllocationManager VmaAllocationManager::create(vk::PhysicalDevice physical_device, vk::Device device,
                                                  vk::Instance instance) {
  auto allocator_info           = VmaAllocatorCreateInfo{};
  allocator_info.physicalDevice = physical_device;
  allocator_info.device         = device;
  allocator_info.instance       = instance;
  allocator_info.flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VmaAllocator allocator = nullptr;
  vmaCreateAllocator(&allocator_info, &allocator);

  return VmaAllocationManager(allocator);
}

Result<VmaBuffer, Error> VmaAllocationManager::create_buffer(const vk::BufferCreateInfo& buffer_create_info,
                                                             const VmaAllocationCreateInfo& alloc_create_info,
                                                             VmaAllocationInfo& out_alloc_info) {
  VkBuffer buf        = VK_NULL_HANDLE;
  VmaAllocation alloc = nullptr;
  VkResult res        = vmaCreateBuffer(allocator_, reinterpret_cast<const VkBufferCreateInfo*>(&buffer_create_info),
                                        &alloc_create_info, &buf, &alloc, &out_alloc_info);
  if (res != VK_SUCCESS) {
    return std::unexpected(Error{
        .msg     = "Failed to allocate a buffer",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = vk::Result(res),
    });
  }

  auto vma_buff = VmaBuffer{
      .vk_buffer  = buf,
      .allocation = alloc,
  };
  vma_objects_.emplace(vma_buff);

  return vma_buff;
}

Result<VmaImage, Error> VmaAllocationManager::create_image(const vk::ImageCreateInfo& image_create_info,
                                                           const VmaAllocationCreateInfo& alloc_create_info,
                                                           VmaAllocationInfo& out_alloc_info) {
  VkImage vkimg{};
  VmaAllocation alloc{};
  auto result = vmaCreateImage(allocator_, reinterpret_cast<const VkImageCreateInfo*>(&image_create_info),
                               &alloc_create_info, &vkimg, &alloc, &out_alloc_info);
  if (result != VK_SUCCESS) {
    return std::unexpected(Error{
        .msg     = "Failed to allocate an image",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = vk::Result(result),
    });
  }
  auto vma_img = VmaImage{
      .vk_image   = vkimg,
      .allocation = alloc,
  };
  vma_objects_.emplace(vma_img);

  return vma_img;
}

void VmaAllocationManager::destroy() {
  for (const auto& o : vma_objects_) {
    std::visit(util::match{
                   [this](VmaBuffer buffer) { vmaDestroyBuffer(allocator_, buffer.vk_buffer, buffer.allocation); },
                   [this](VmaImage image) { vmaDestroyImage(allocator_, image.vk_image, image.allocation); },
               },
               o);
  }
  vmaDestroyAllocator(allocator_);
  allocator_ = nullptr;
}

VmaAllocationManager::~VmaAllocationManager() {
  if (allocator_ != nullptr) {
    destroy();
  }
}

void VmaAllocationManager::delete_buffer(VmaBuffer buffer) {
  vma_objects_.erase(buffer);
  vmaDestroyBuffer(allocator_, buffer.vk_buffer, buffer.allocation);
}

void VmaAllocationManager::delete_image(VmaImage image) {
  vma_objects_.erase(image);
  vmaDestroyImage(allocator_, image.vk_image, image.allocation);
}

Result<VmaBuffer, Error> VmaAllocationManager::create_buffer(const vk::BufferCreateInfo& buffer_create_info,
                                                             const VmaAllocationCreateInfo& alloc_create_info) {
  VmaAllocationInfo info;
  return create_buffer(buffer_create_info, alloc_create_info, info);
}

Result<VmaImage, Error> VmaAllocationManager::create_image(const vk::ImageCreateInfo& image_create_info,
                                                           const VmaAllocationCreateInfo& alloc_create_info) {
  VmaAllocationInfo info;
  return create_image(image_create_info, alloc_create_info, info);
}

}  // namespace eray::vkren
