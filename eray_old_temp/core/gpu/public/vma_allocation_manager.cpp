#include <liberay/util/variant_match.hpp>
#include <liberay/vkren/vma_allocation_manager.hpp>

namespace eray::vkren {

VmaAllocationManager::VmaAllocationManager(VmaAllocationManager&& other) noexcept
    : allocator_(other.allocator_), vma_objects_(std::move(other.vma_objects_)) {
  other.allocator_ = nullptr;
}

VmaAllocationManager& VmaAllocationManager::operator=(VmaAllocationManager&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  if (allocator_ != nullptr) {
    destroy();
  }
  allocator_       = other.allocator_;
  vma_objects_     = std::move(other.vma_objects_);
  other.allocator_ = nullptr;

  return *this;
}

Result<VmaAllocationManager, Error> VmaAllocationManager::create(vk::PhysicalDevice physical_device, vk::Device device,
                                                                 vk::Instance instance) {
  auto allocator_info           = VmaAllocatorCreateInfo{};
  allocator_info.physicalDevice = physical_device;
  allocator_info.device         = device;
  allocator_info.instance       = instance;
  allocator_info.flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VmaAllocator allocator = nullptr;

  auto result = vk::Result(vmaCreateAllocator(&allocator_info, &allocator));

  if (result != vk::Result::eSuccess) {
    util::Logger::err("Failed to create a VMA Allocation Manager.");
    return std::unexpected(Error{
        .msg     = "VMA Allocation Manager creation failed",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = result,
    });
  }

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
      .vk_buffer  = vk::Buffer(buf),
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
      .vk_image   = vk::Image(vkimg),
      .allocation = alloc,
  };
  vma_objects_.emplace(vma_img);

  return vma_img;
}

void VmaAllocationManager::destroy() {
  assert(allocator_ != nullptr && "Allocator must not be NULL");

  for (const auto& o : vma_objects_) {
    std::visit(util::match{
                   [this](VmaBuffer buffer) {
                     vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(buffer.vk_buffer), buffer.allocation);
                   },
                   [this](VmaImage image) {
                     vmaDestroyImage(allocator_, static_cast<VkImage>(image.vk_image), image.allocation);
                   },
               },
               o);
  }
  vma_objects_.clear();
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
  vmaDestroyBuffer(allocator_, static_cast<VkBuffer>(buffer.vk_buffer), buffer.allocation);
}

void VmaAllocationManager::delete_image(VmaImage image) {
  vma_objects_.erase(image);
  vmaDestroyImage(allocator_, static_cast<VkImage>(image.vk_image), image.allocation);
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
