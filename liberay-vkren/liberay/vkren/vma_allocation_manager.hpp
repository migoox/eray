#pragma once

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/vma_object.hpp>
#include <unordered_set>
#include <variant>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace eray::vkren {

class VmaAllocationManager {
 public:
  VmaAllocationManager() = delete;
  explicit VmaAllocationManager(std::nullptr_t) {}

  static VmaAllocationManager create(vk::PhysicalDevice physical_device, vk::Device device, vk::Instance instance);

  ~VmaAllocationManager();

  VmaAllocator allocator() const { return allocator_; }

  [[nodiscard]] Result<VmaBuffer, Error> create_buffer(const vk::BufferCreateInfo& buffer_create_info,
                                                       const VmaAllocationCreateInfo& alloc_create_info,
                                                       VmaAllocationInfo& out_alloc_info);

  [[nodiscard]] Result<VmaImage, Error> create_image(const vk::ImageCreateInfo& image_create_info,
                                                     const VmaAllocationCreateInfo& alloc_create_info,
                                                     VmaAllocationInfo& out_alloc_info);

  [[nodiscard]] Result<VmaBuffer, Error> create_buffer(const vk::BufferCreateInfo& buffer_create_info,
                                                       const VmaAllocationCreateInfo& alloc_create_info);

  [[nodiscard]] Result<VmaImage, Error> create_image(const vk::ImageCreateInfo& image_create_info,
                                                     const VmaAllocationCreateInfo& alloc_create_info);

  void delete_buffer(VmaBuffer buffer);
  void delete_image(VmaImage image);

  void destroy();

 private:
  explicit VmaAllocationManager(VmaAllocator allocator) : allocator_(allocator) {}
  using VmaObjectVariant = std::variant<VmaImage, VmaBuffer>;

  VmaAllocator allocator_ = nullptr;
  std::unordered_set<VmaObjectVariant> vma_objects_;
};

}  // namespace eray::vkren
