#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/resource.hpp>
#include <liberay/vkren/vma_object.hpp>
#include <unordered_set>
#include <variant>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace eray::vkren {

enum class ImageType : uint8_t {
  Color,
  DepthStencil,
  Depth,
  Stencil,
  ShaderStorageImage,
};

struct BufferResourceEntry {
  VmaBuffer buffer;
  VmaAllocation allocation;
  vk::DeviceSize size_bytes;
  vk::BufferUsageFlags usage;
};

struct ImageResourceEntry {
  ImageDescription description;
  vk::ImageAspectFlags aspect;
  VmaImage image;
  VmaAllocation allocation;
  uint32_t mip_levels;
  vk::ImageUsageFlags usage;
  vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1;
  ImageType type;
  vk::ImageView image_view;  // lazy, create it on demand
};

struct SamplerResourceEntry {
  vk::Sampler sampler;
};

class ResourceManager {
 public:
  ResourceManager() = delete;
  explicit ResourceManager(std::nullptr_t) {}

  ResourceManager(ResourceManager&&) noexcept;
  ResourceManager& operator=(ResourceManager&&) noexcept;
  ResourceManager(const ResourceManager&)            = delete;
  ResourceManager& operator=(const ResourceManager&) = delete;

  ~ResourceManager();

  static Result<ResourceManager, Error> create(vk::PhysicalDevice physical_device, vk::Device device,
                                               vk::Instance instance);

  [[nodiscard]] Result<BufferHandle, Error> create_buffer(const vk::BufferCreateInfo& buffer_create_info,
                                                          const VmaAllocationCreateInfo& alloc_create_info,
                                                          VmaAllocationInfo& out_alloc_info) noexcept;

  [[nodiscard]] Result<BufferHandle, Error> create_buffer(const vk::BufferCreateInfo& buffer_create_info,
                                                          const VmaAllocationCreateInfo& alloc_create_info) noexcept;

  [[nodiscard]] Result<ImageHandle, Error> create_image(const vk::ImageCreateInfo& image_create_info,
                                                        const VmaAllocationCreateInfo& alloc_create_info,
                                                        VmaAllocationInfo& out_alloc_info) noexcept;

  [[nodiscard]] Result<ImageHandle, Error> create_image(const vk::ImageCreateInfo& image_create_info,
                                                        const VmaAllocationCreateInfo& alloc_create_info) noexcept;

  /**
   * @brief A "staging" buffer than you want to map and fill from CPU code, then use as a source of transfer to some GPU
   * resource.
   *
   * @param device
   * @param size_bytes
   * @return Result<Buffer, Error>
   */
  [[nodiscard]] Result<BufferHandle, Error> create_staging_buffer(vk::Device device,
                                                                  const util::MemoryRegion& src_region);

  /**
   * @brief Creates a gpu local buffer, e.g. for vertex or index buffer.
   *
   * @param device
   * @param size_bytes
   * @param usage
   * @return Result<Buffer, Error>
   */
  [[nodiscard]] Result<BufferHandle, Error> create_gpu_local_buffer(vk::Device device, vk::DeviceSize size_bytes,
                                                                    vk::BufferUsageFlags usage);

  [[nodiscard]] Result<BufferHandle, Error> create_index_buffer(vk::Device device, vk::DeviceSize size_bytes) {
    return create_gpu_local_buffer(device, size_bytes, vk::BufferUsageFlagBits::eIndexBuffer);
  }

  [[nodiscard]] Result<BufferHandle, Error> create_vertex_buffer(vk::Device device, vk::DeviceSize size_bytes) {
    return create_gpu_local_buffer(device, size_bytes, vk::BufferUsageFlagBits::eVertexBuffer);
  }

  /**
   * @brief For resources that you frequently write on CPU via mapped pointer and frequently read on GPU e.g. uniform
   * buffer (also called "dynamic"). This buffer might not be mappable, to upload data safely use `write()`.
   *
   * @return Result<Buffer, Error>
   */
  [[nodiscard]] Result<BufferHandle, Error> create_uniform_buffer(vk::Device device, vk::DeviceSize size_bytes);

  [[nodiscard]] Result<BufferHandle, Error> create_storage_buffer(vk::Device device, vk::DeviceSize size_bytes) {
    return create_gpu_local_buffer(device, size_bytes, vk::BufferUsageFlagBits::eStorageBuffer);
  }

  /**
   * @brief Creates a temporary staging buffer and uses it to fill the buffer.
   * This function blocks the CPU until the write is ready.
   *
   * @return Result<void, Error>
   */
  Result<void, Error> buffer_write_via_staging_buffer(BufferHandle dst, const util::MemoryRegion& src_region) const;

  /**
   * @brief Uses provided staging buffer to fill the buffer.
   * This function blocks the CPU until the write is ready.
   *
   * @return Result<void, Error>
   */
  Result<void, Error> buffer_write_via_staging_buffer(BufferHandle dst, BufferHandle staging_buffer,
                                                      const util::MemoryRegion& src_region) const;

  /**
   * @brief Fills the buffer. If the buffer is not mappable it will call `write_via_staging_buffer()`.
   * This function blocks the CPU until the write is ready.
   *
   * @return Result<void, Error>
   */
  Result<void, Error> buffer_write(BufferHandle dst, const util::MemoryRegion& src_region) const;

  /**
   * @brief Copies data between GPU buffers.
   *
   * @param buffer
   * @param offset
   */
  void buffer_cpy(BufferHandle dst, BufferHandle src) const;

  /**
   * @brief Maps the buffer and returns a pointer to the mapped memory. The caller is responsible for unmapping the
   * buffer after use.
   */
  Result<void*, Error> buffer_map(BufferHandle buffer) const;

  /**
   * @brief Unmaps the buffer. The caller should call this function after finishing using the mapped memory from
   * `map()`.
   *
   */
  void buffer_unmap(BufferHandle buffer) const;

  vk::DeviceSize get_buffer_size_bytes(BufferHandle buffer) const;

  vk::Buffer get_buffer_vk(BufferHandle buffer) const;

  vk::DescriptorBufferInfo get_buffer_descriptor_info(BufferHandle buffer, vk::DeviceSize offset = 0,
                                                         vk::DeviceSize range = vk::WholeSize) const;

  void delete_buffer(BufferHandle buffer);

  [[nodiscard]] Result<ImageHandle, Error> create_attachment_image(
      vk::Device device, const ImageDescription& desc, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1);

  [[nodiscard]] Result<ImageHandle, Error> create_color_attachment_image(
      vk::Device device, const ImageDescription& desc,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return create_attachment_image(
        device, desc, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
        vk::ImageAspectFlagBits::eColor, sample_count);
  }

  [[nodiscard]] Result<ImageHandle, Error> create_depth_stencil_attachment_image(
      vk::Device device, const ImageDescription& desc,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return create_attachment_image(device, desc, vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                   vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil, sample_count);
  }

  [[nodiscard]] Result<ImageHandle, Error> create_stencil_attachment_image(
      vk::Device device, const ImageDescription& desc,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return create_attachment_image(device, desc, vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                   vk::ImageAspectFlagBits::eStencil, sample_count);
  }

  [[nodiscard]] Result<ImageHandle, Error> create_depth_attachment_image(
      vk::Device device, const ImageDescription& desc,
      vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return create_attachment_image(device, desc, vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                   vk::ImageAspectFlagBits::eDepth, sample_count);
  }

  [[nodiscard]] Result<ImageHandle, Error> create_texture_image(
      vk::Device device, ImageDescription desc, bool mipmapping = true,
      vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

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
  Result<void, Error> image_write(ImageHandle dst, util::MemoryRegion src_region);

  vk::Extent3D get_image_extent(ImageHandle image) const;

  vk::ImageSubresourceLayers get_image_subresource_layers(ImageHandle image, uint32_t mip_level = 0,
                                                         uint32_t base_layer = 0, uint32_t layer_count = 1) const;

  vk::ImageSubresourceRange get_image_full_resource_range() const;

  /**
   * @brief In case of `vk::ImageType::e2D` returns image view with `vk::ImageViewType::e2D`. Returns image view
   * `vk::ImageViewType::e3D` otherwise.
   *
   * @return Result<vk::raii::ImageView, Error>
   */
  vk::ImageView get_image_view(ImageHandle image) const;

  bool get_image_mip_levels(ImageHandle image);

  void delete_image(ImageHandle image);

  void destroy();

 private:
  VmaAllocator allocator_ = nullptr;

  SlotMap<BufferHandle, BufferResourceEntry> buffer_pool_;
  SlotMap<ImageHandle, ImageResourceEntry> image_pool_;
  SlotMap<SamplerHandle, SamplerResourceEntry> sampler_pool_;
};

}  // namespace eray::vkren