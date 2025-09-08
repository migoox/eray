#pragma once

#include <cstddef>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

struct DescriptorPoolSizeRatio {
  vk::DescriptorType type;

  /**
   * @brief When you say ratio = 3.0F, for UniformBuffer, that means "on average, each set I allocate will use
   * approximately ~3 uniform buffers".
   *
   */
  float ratio;

  static std::vector<DescriptorPoolSizeRatio> create_standard_ratios(float storage_image_ratio,
                                                                     float storage_buffer_ratio,
                                                                     float uniform_buffer_ratio,
                                                                     float combined_image_sampler_ratio);
};

class DescriptorAllocator {
 public:
  DescriptorAllocator() = delete;
  explicit DescriptorAllocator(std::nullptr_t) {}

  DescriptorAllocator(const DescriptorAllocator&)                = delete;
  DescriptorAllocator(DescriptorAllocator&&) noexcept            = default;
  DescriptorAllocator& operator=(const DescriptorAllocator&)     = delete;
  DescriptorAllocator& operator=(DescriptorAllocator&&) noexcept = default;

  static DescriptorAllocator create(Device& device);
  static Result<DescriptorAllocator, Error> create_and_init(Device& device, uint32_t max_sets,
                                                            std::span<DescriptorPoolSizeRatio> pool_size_ratios);

  /**
   * @brief
   *
   * @param max_sets Max sets per pool.
   * @param pool_size_ratios
   * @return Result<void, Error>
   */
  Result<void, Error> init(uint32_t max_sets, std::span<DescriptorPoolSizeRatio> pool_size_ratios);

  void clear_pools();
  void destroy_pools();

  Result<vk::DescriptorSet, Error> allocate(vk::DescriptorSetLayout layout, void* p_next = nullptr);

 private:
  explicit DescriptorAllocator(Device& device) : p_device_(&device) {}

  Result<vk::raii::DescriptorPool, Error> get_pool();
  Result<vk::raii::DescriptorPool, Error> create_pool(uint32_t set_count,
                                                      std::span<DescriptorPoolSizeRatio> pool_ratios);

  std::vector<DescriptorPoolSizeRatio> ratios_;

  /**
   * @brief Contains the pools we know we cant allocate from anymore.
   *
   */
  std::vector<vk::raii::DescriptorPool> full_pools_;

  /**
   * @brief Contains pools that can still be used, or the freshly created ones.
   *
   */
  std::vector<vk::raii::DescriptorPool> ready_pools_;
  uint32_t sets_per_pool_{};

  observer_ptr<Device> p_device_{};
};

struct DescriptorSetWriter {
  std::deque<vk::DescriptorImageInfo> image_infos;
  std::deque<vk::DescriptorBufferInfo> buffer_infos;
  std::vector<vk::WriteDescriptorSet> writes;
  observer_ptr<Device> _p_device;

  static DescriptorSetWriter create(Device& device);

  /**
   * @brief Calls the `write_image` function with VK_DESCRIPTOR_TYPE_SAMPLER type.
   *
   * @param binding
   * @param sampler
   */
  void write_sampler(uint32_t binding, vk::Sampler sampler);

  /**
   * @brief Calls the `write_image` function with VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE type.
   *
   * @param binding
   * @param image
   * @param layout
   */
  void write_sampled_image(uint32_t binding, vk::ImageView image, vk::ImageLayout layout);

  /**
   * @brief Calls the `write_image` function with VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER type.
   *
   * @param binding
   * @param image
   * @param sampler
   * @param layout
   */
  void write_combined_image_sampler(uint32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout);

  /**
   * @brief Calls the `write_image` function with VK_DESCRIPTOR_TYPE_STORAGE_IMAGE type.
   *
   * @param binding
   * @param image
   * @param layout
   */
  void write_storage_image(uint32_t binding, vk::ImageView image, vk::ImageLayout layout);

  /**
   * @brief Generalized image write. It's abstracted by `write_sampler`, `write_sampled_image`,
   * `write_combined_image_sampler` and `write_storage_image`.
   *
   * @param binding
   * @param image
   * @param sampler
   * @param layout
   * @param type
   */
  void write_image(uint32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout,
                   vk::DescriptorType type);

  void write_buffer(uint32_t binding, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type);
  void write_buffer(uint32_t binding, vk::DescriptorBufferInfo info, vk::DescriptorType type);

  void clear();
  void update_set(vk::DescriptorSet set);
};

}  // namespace eray::vkren
