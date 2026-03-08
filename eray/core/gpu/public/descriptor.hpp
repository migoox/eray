#pragma once

#include <cstddef>
#include <deque>
#include <liberay/vkren/common.hpp>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

class Device;

struct DescriptorPoolSizeRatio {
  vk::DescriptorType type;

  /**
   * @brief When you say ratio = 3.0F, for UniformBuffer, that means "on average, each set I allocate will use
   * approximately ~3 uniform buffers".
   *
   */
  float ratio;

  /**
   * @brief Reasonable default, but you can improve memory usage of the `DescriptorAllocator` significantly if
   * you tweak it to what your project uses.
   *
   * @return std::vector<DescriptorPoolSizeRatio>
   */
  static std::vector<DescriptorPoolSizeRatio> create_default();
};

class DescriptorAllocator {
 public:
  DescriptorAllocator() = delete;
  explicit DescriptorAllocator(std::nullptr_t) {}

  DescriptorAllocator(const DescriptorAllocator&)                = delete;
  DescriptorAllocator(DescriptorAllocator&&) noexcept            = default;
  DescriptorAllocator& operator=(const DescriptorAllocator&)     = delete;
  DescriptorAllocator& operator=(DescriptorAllocator&&) noexcept = default;
  ~DescriptorAllocator();

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

  void clear();
  void destroy();

  Result<vk::DescriptorSet, Error> allocate(vk::DescriptorSetLayout layout, void* p_next = nullptr);
  Result<std::vector<vk::DescriptorSet>, Error> allocate_many(vk::DescriptorSetLayout layout, size_t count,
                                                              void* p_next = nullptr);

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

  // TODO(migoox): Individual descriptor set deallocation mechanism
  std::vector<vk::raii::DescriptorSet> allocated_descriptors_;

  observer_ptr<Device> p_device_{};
};

struct DescriptorSetBinder {
  std::deque<vk::DescriptorImageInfo> image_infos;
  std::deque<vk::DescriptorBufferInfo> buffer_infos;
  std::vector<vk::WriteDescriptorSet> writes;
  observer_ptr<Device> _p_device;

  static DescriptorSetBinder create(Device& device);

  /**
   * @brief Calls the `bind_image` function with VK_DESCRIPTOR_TYPE_SAMPLER type.
   *
   * @param binding
   * @param sampler
   */
  void bind_sampler(uint32_t binding, vk::Sampler sampler);

  /**
   * @brief Calls the `bind_image` function with VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE type.
   *
   * @param binding
   * @param image
   * @param layout
   */
  void bind_sampled_image(uint32_t binding, vk::ImageView image, vk::ImageLayout layout);

  /**
   * @brief Calls the `bind_image` function with VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER type.
   *
   * @param binding
   * @param image
   * @param sampler
   * @param layout
   */
  void bind_combined_image_sampler(uint32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout);

  /**
   * @brief Calls the `bind_image` function with VK_DESCRIPTOR_TYPE_STORAGE_IMAGE type.
   *
   * @param binding
   * @param image
   * @param layout
   */
  void bind_storage_image(uint32_t binding, vk::ImageView image, vk::ImageLayout layout);

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
  void bind_image(uint32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout,
                  vk::DescriptorType type);

  void bind_buffer(uint32_t binding, vk::Buffer buffer, size_t size, size_t offset, vk::DescriptorType type);
  void bind_buffer(uint32_t binding, vk::DescriptorBufferInfo info, vk::DescriptorType type);

  void clear();
  void apply(vk::DescriptorSet ds);
  void apply_and_clear(vk::DescriptorSet ds) {
    apply(ds);
    clear();
  }
};

/**
 * @brief Stores a collection of layout binding descriptions that can be hashed and compared.
 *
 */
struct DescriptorSetLayoutInfo {
  // TODO(migoox): Use static array instead of the vector
  //   static constexpr size_t kMaxBindings = 16;
  std::vector<vk::DescriptorSetLayoutBinding> bindings;
  size_t _hash{};

  DescriptorSetLayoutInfo() = delete;
  static DescriptorSetLayoutInfo create(std::vector<vk::DescriptorSetLayoutBinding>&& bindings);

  bool operator==(const DescriptorSetLayoutInfo& other) const;

  struct Hash {
    std::size_t operator()(const DescriptorSetLayoutInfo& dsl) const { return dsl._hash; }
  };

 private:
  size_t generate_hash() const;

  explicit DescriptorSetLayoutInfo(std::vector<vk::DescriptorSetLayoutBinding>&& bindings)
      : bindings(std::move(bindings)) {}
};

/**
 * @brief Instead of reusing the same descriptor set layouts through the codebase manually, use this class. If specific
 * layout has already been created, it allows to reuse it.
 *
 */
struct DescriptorSetLayoutManager {
  DescriptorSetLayoutManager() = delete;
  explicit DescriptorSetLayoutManager(std::nullptr_t) {}

  using LayoutCacheMap =
      std::unordered_map<DescriptorSetLayoutInfo, vk::raii::DescriptorSetLayout, DescriptorSetLayoutInfo::Hash>;

  LayoutCacheMap _layout_cache;
  observer_ptr<Device> _p_device{};

  static DescriptorSetLayoutManager create(Device& device);
  [[nodiscard]] Result<vk::DescriptorSetLayout, Error> create_layout(
      const vk::DescriptorSetLayoutCreateInfo& create_info);
  void destroy();

 private:
  explicit DescriptorSetLayoutManager(Device& device);
};

struct DescriptorSetBuilder {
  DescriptorSetBuilder() = delete;
  [[nodiscard]] static DescriptorSetBuilder create(DescriptorSetLayoutManager& layout_manager,
                                                   DescriptorAllocator& allocator);
  [[nodiscard]] static DescriptorSetBuilder create(Device& device);

  /**
   * @brief Order of the bindings specifies the binding numbers.
   *
   * @param type
   * @param stage_flags
   * @return DescriptorSetBuilder&
   */
  DescriptorSetBuilder& with_binding(vk::DescriptorType type, vk::ShaderStageFlags stage_flags, uint32_t count = 1);

  Result<vk::DescriptorSetLayout, Error> build_layout_only();

  struct DescriptorSet {
    vk::DescriptorSet descriptor_set;
    vk::DescriptorSetLayout layout;
  };
  Result<DescriptorSet, Error> build();

  struct DescriptorSets {
    std::vector<vk::DescriptorSet> descriptor_sets;
    vk::DescriptorSetLayout layout;
  };
  Result<DescriptorSets, Error> build_many(size_t count);

  std::vector<vk::DescriptorSetLayoutBinding> bindings;

  observer_ptr<DescriptorSetLayoutManager> _dsl_manager;
  observer_ptr<DescriptorAllocator> _allocator;
  uint32_t _binding_count = 0;

 private:
  explicit DescriptorSetBuilder(DescriptorSetLayoutManager& layout_manager, DescriptorAllocator& allocator);
};

}  // namespace eray::vkren
