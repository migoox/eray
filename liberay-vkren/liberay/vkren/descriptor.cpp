#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <expected>
#include <liberay/vkren/descriptor.hpp>
#include <liberay/vkren/error.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

std::vector<DescriptorPoolSizeRatio> DescriptorPoolSizeRatio::create_default() {
  return {{.type = vk::DescriptorType::eSampler, .ratio = 0.5F},
          {.type = vk::DescriptorType::eCombinedImageSampler, .ratio = 4.0F},
          {.type = vk::DescriptorType::eSampledImage, .ratio = 4.0F},
          {.type = vk::DescriptorType::eStorageImage, .ratio = 1.0F},
          {.type = vk::DescriptorType::eUniformTexelBuffer, .ratio = 1.0F},
          {.type = vk::DescriptorType::eStorageTexelBuffer, .ratio = 1.0F},
          {.type = vk::DescriptorType::eUniformBuffer, .ratio = 2.0F},
          {.type = vk::DescriptorType::eStorageBuffer, .ratio = 2.0F},
          {.type = vk::DescriptorType::eUniformBufferDynamic, .ratio = 1.0F},
          {.type = vk::DescriptorType::eStorageBufferDynamic, .ratio = 1.0F},
          {.type = vk::DescriptorType::eInputAttachment, .ratio = 0.5F}};
}

void DescriptorAllocator::clear() {
  ready_pools_.insert(ready_pools_.end(), std::make_move_iterator(full_pools_.begin()),
                      std::make_move_iterator(full_pools_.end()));
  full_pools_.clear();
}

void DescriptorAllocator::destroy() {
  allocated_descriptors_.clear();
  ready_pools_.clear();
  full_pools_.clear();
}

Result<vk::raii::DescriptorPool, Error> DescriptorAllocator::get_pool() {
  if (!ready_pools_.empty()) {
    auto new_pool = std::move(ready_pools_.back());
    ready_pools_.pop_back();
    return new_pool;
  }

  auto new_pool = create_pool(sets_per_pool_, ratios_);
  if (!new_pool) {
    return std::unexpected(new_pool.error());
  }

  sets_per_pool_ = sets_per_pool_ + sets_per_pool_ / 2;
  sets_per_pool_ = std::min<uint32_t>(sets_per_pool_, 4092);
  return std::move(*new_pool);
}

Result<vk::raii::DescriptorPool, Error> DescriptorAllocator::create_pool(
    uint32_t set_count, std::span<DescriptorPoolSizeRatio> pool_ratios) {
  auto pool_sizes = std::vector<vk::DescriptorPoolSize>();
  for (DescriptorPoolSizeRatio ratio : pool_ratios) {
    pool_sizes.push_back(vk::DescriptorPoolSize{
        .type            = ratio.type,
        .descriptorCount = std::max(1U, static_cast<uint32_t>(static_cast<float>(set_count) * ratio.ratio)),
    });
  }

  auto create_info = vk::DescriptorPoolCreateInfo{
      .sType = vk::StructureType::eDescriptorPoolCreateInfo,
      // TODO(migoox): Resarch why eFreeDescriptorSet is needed
      .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets       = set_count,
      .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
      .pPoolSizes    = pool_sizes.data(),
  };

  auto pool_opt = Result((*p_device_)->createDescriptorPool(create_info));
  if (!pool_opt) {
    return std::unexpected(Error{
        .msg     = "Descriptor Pool creation failed",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = pool_opt.error(),
    });
  }

  return std::move(*pool_opt);
}

DescriptorAllocator DescriptorAllocator::create(Device& device) { return DescriptorAllocator(device); }

Result<DescriptorAllocator, Error> DescriptorAllocator::create_and_init(
    Device& device, uint32_t max_sets, std::span<DescriptorPoolSizeRatio> pool_size_ratios) {
  auto allocator = DescriptorAllocator(device);
  return allocator.init(max_sets, pool_size_ratios).transform([&]() { return std::move(allocator); });
}

Result<void, Error> DescriptorAllocator::init(uint32_t max_sets, std::span<DescriptorPoolSizeRatio> pool_size_ratios) {
  ratios_.clear();

  for (auto r : pool_size_ratios) {
    ratios_.push_back(r);
  }

  auto new_pool = create_pool(max_sets, ratios_);
  if (!new_pool) {
    return std::unexpected(new_pool.error());
  }

  sets_per_pool_ = max_sets;
  ready_pools_.emplace_back(std::move(*new_pool));

  return {};
}

DescriptorAllocator::~DescriptorAllocator() { destroy(); }

Result<vk::DescriptorSet, Error> DescriptorAllocator::allocate(vk::DescriptorSetLayout layout, void* p_next) {
  return allocate_many(layout, 1, p_next).transform([](auto&& val) { return std::move(val.front()); });
}

Result<std::vector<vk::DescriptorSet>, Error> DescriptorAllocator::allocate_many(vk::DescriptorSetLayout layout,
                                                                                 size_t count, void* p_next) {
  assert(count > 0 && "Descriptor Set count must be greater than 0");

  auto pool_to_use = get_pool();
  if (!pool_to_use) {
    return std::unexpected(pool_to_use.error());
  }

  auto layouts    = std::ranges::views::repeat(layout, count) | std::ranges::to<std::vector>();
  auto alloc_info = vk::DescriptorSetAllocateInfo{
      .sType              = vk::StructureType::eDescriptorSetAllocateInfo,
      .pNext              = p_next,
      .descriptorPool     = *pool_to_use,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts        = layouts.data(),
  };

  using Expected = std::expected<std::vector<vk::raii::DescriptorSet>, Error>;

  auto ds_opt = (*p_device_)->allocateDescriptorSets(alloc_info).or_else([&](auto&& err) -> Expected {
    if (err == vk::Result::eErrorOutOfPoolMemory || err == vk::Result::eErrorFragmentedPool) {
      full_pools_.push_back(std::move(*pool_to_use));
      auto retry_pool = get_pool();
      if (!retry_pool) {
        return std::unexpected(retry_pool.error());
      }

      pool_to_use               = std::move(retry_pool);
      alloc_info.descriptorPool = *pool_to_use;

      return (*p_device_)->allocateDescriptorSets(alloc_info).or_else([](auto&& retry_err) -> Expected {
        return std::unexpected(Error{
            .msg     = "Descriptor Sets creation failure",
            .code    = ErrorCode::VulkanObjectCreationFailure{},
            .vk_code = retry_err,
        });
      });
    }

    return std::unexpected(Error{
        .msg     = "Descriptor Sets creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = err,
    });
  });

  if (!ds_opt) {
    full_pools_.push_back(std::move(*pool_to_use));
    return std::unexpected(ds_opt.error());
  }

  ready_pools_.push_back(std::move(*pool_to_use));

  auto result = std::vector<vk::DescriptorSet>();
  result.reserve(count);
  for (auto& ds : *ds_opt) {
    result.push_back(*ds);
  }

  allocated_descriptors_.insert(allocated_descriptors_.end(), std::make_move_iterator(ds_opt->begin()),
                                std::make_move_iterator(ds_opt->end()));

  return result;
}

void DescriptorSetWriter::write_sampler(uint32_t binding, vk::Sampler sampler) {
  write_image(binding, VK_NULL_HANDLE, sampler, vk::ImageLayout::eUndefined, vk::DescriptorType::eSampler);
}

void DescriptorSetWriter::write_sampled_image(uint32_t binding, vk::ImageView image, vk::ImageLayout layout) {
  write_image(binding, image, VK_NULL_HANDLE, layout, vk::DescriptorType::eSampledImage);
}

void DescriptorSetWriter::write_combined_image_sampler(uint32_t binding, vk::ImageView image, vk::Sampler sampler,
                                                       vk::ImageLayout layout) {
  write_image(binding, image, sampler, layout, vk::DescriptorType::eCombinedImageSampler);
}

void DescriptorSetWriter::write_storage_image(uint32_t binding, vk::ImageView image, vk::ImageLayout layout) {
  write_image(binding, image, VK_NULL_HANDLE, layout, vk::DescriptorType::eStorageImage);
}

void DescriptorSetWriter::write_image(uint32_t binding, vk::ImageView image, vk::Sampler sampler,
                                      vk::ImageLayout layout, vk::DescriptorType type) {
  auto& info = image_infos.emplace_back(vk::DescriptorImageInfo{
      .sampler     = sampler,
      .imageView   = image,
      .imageLayout = layout,
  });

  auto write = vk::WriteDescriptorSet{
      .sType           = vk::StructureType::eWriteDescriptorSet,
      .dstSet          = VK_NULL_HANDLE,
      .dstBinding      = binding,
      .descriptorCount = 1,
      .descriptorType  = type,
      .pImageInfo      = &info,
  };

  writes.push_back(write);
}

void DescriptorSetWriter::write_buffer(uint32_t binding, vk::DescriptorBufferInfo info, vk::DescriptorType type) {
  write_buffer(binding, info.buffer, info.range, info.offset, type);
}

void DescriptorSetWriter::write_buffer(uint32_t binding, vk::Buffer buffer, size_t size, size_t offset,
                                       vk::DescriptorType type) {
  auto& info = buffer_infos.emplace_back(vk::DescriptorBufferInfo{
      .buffer = buffer,
      .offset = offset,
      .range  = size,
  });

  auto write = vk::WriteDescriptorSet{
      .sType           = vk::StructureType::eWriteDescriptorSet,
      .dstSet          = VK_NULL_HANDLE,
      .dstBinding      = binding,
      .descriptorCount = 1,
      .descriptorType  = type,
      .pBufferInfo     = &info,
  };

  writes.push_back(write);
}

void DescriptorSetWriter::clear() {
  image_infos.clear();
  writes.clear();
  buffer_infos.clear();
}

void DescriptorSetWriter::write_to_set(vk::DescriptorSet set) {
  for (auto& write : writes) {
    write.dstSet = set;
  }

  (*_p_device)->updateDescriptorSets(writes, nullptr);
}

DescriptorSetLayoutInfo DescriptorSetLayoutInfo::create(std::vector<vk::DescriptorSetLayoutBinding>&& bindings) {
  auto dsl  = DescriptorSetLayoutInfo(std::move(bindings));
  dsl._hash = dsl.generate_hash();
  return dsl;
}

bool DescriptorSetLayoutInfo::operator==(const DescriptorSetLayoutInfo& other) const {
  if (other.bindings.size() != bindings.size()) {
    return false;
  }

  // compare each of the bindings is the same. Bindings are sorted so they will match
  for (auto i = 0U; i < bindings.size(); i++) {
    if (other.bindings[i].binding != bindings[i].binding) {
      return false;
    }
    if (other.bindings[i].descriptorType != bindings[i].descriptorType) {
      return false;
    }
    if (other.bindings[i].descriptorCount != bindings[i].descriptorCount) {
      return false;
    }
    if (other.bindings[i].stageFlags != bindings[i].stageFlags) {
      return false;
    }
  }

  return true;
}

size_t DescriptorSetLayoutInfo::generate_hash() const {
  using std::hash;
  using std::size_t;

  size_t result = hash<size_t>()(bindings.size());

  for (const vk::DescriptorSetLayoutBinding& b : bindings) {
    size_t binding_hash = b.binding | (static_cast<uint32_t>(b.descriptorType) << 8) | (b.descriptorCount << 16) |
                          (static_cast<uint32_t>(b.stageFlags) << 24);

    result ^= hash<size_t>()(binding_hash);
  }

  return result;
}

DescriptorSetWriter DescriptorSetWriter::create(Device& device) {
  return DescriptorSetWriter{
      .image_infos  = {},
      .buffer_infos = {},
      .writes       = {},
      ._p_device    = &device,
  };
}

DescriptorSetLayoutManager::DescriptorSetLayoutManager(Device& device) : _p_device(&device) {}

DescriptorSetLayoutManager DescriptorSetLayoutManager::create(Device& device) {
  return DescriptorSetLayoutManager(device);
}

Result<vk::DescriptorSetLayout, Error> DescriptorSetLayoutManager::create_layout(
    const vk::DescriptorSetLayoutCreateInfo& create_info) {
  auto binding_comparer = [](const auto& a, const auto& b) { return a.binding < b.binding; };

  const auto input_bindings = std::span{create_info.pBindings, create_info.bindingCount};
  const auto is_sorted      = std::ranges::is_sorted(input_bindings, binding_comparer);

  auto layout_info = DescriptorSetLayoutInfo::create(std::ranges::to<std::vector>(input_bindings));
  if (!is_sorted) {
    std::ranges::sort(layout_info.bindings, binding_comparer);
  }

  // Try to grab from the cache before creating the layout
  auto it = _layout_cache.find(layout_info);
  if (it != _layout_cache.end()) {
    return *(*it).second;
  }

  // Create when not cached
  using Expected = std::expected<vk::DescriptorSetLayout, vk::Result>;
  return (*_p_device)
      ->createDescriptorSetLayout(create_info)
      .and_then([this, &layout_info](vk::raii::DescriptorSetLayout&& dsl) -> Expected {
        _layout_cache.emplace(layout_info, std::move(dsl));
        return *_layout_cache.at(layout_info);
      })
      .transform_error([](auto err) {
        return Error{
            .msg     = "Descriptor Set Layout creation failure",
            .code    = ErrorCode::VulkanObjectCreationFailure{},
            .vk_code = err,
        };
      });
}

void DescriptorSetLayoutManager::destroy() { _layout_cache.clear(); }

DescriptorSetBuilder DescriptorSetBuilder::create(DescriptorSetLayoutManager& layout_manager,
                                                  DescriptorAllocator& allocator) {
  return DescriptorSetBuilder(layout_manager, allocator);
}

DescriptorSetBuilder::DescriptorSetBuilder(DescriptorSetLayoutManager& layout_manager, DescriptorAllocator& allocator)
    : _dsl_manager(&layout_manager), _allocator(&allocator) {}

DescriptorSetBuilder& DescriptorSetBuilder::with_binding(vk::DescriptorType type, vk::ShaderStageFlags stage_flags) {
  auto binding = vk::DescriptorSetLayoutBinding{
      .binding            = _binding_count++,
      .descriptorType     = type,
      .descriptorCount    = 1,
      .stageFlags         = stage_flags,
      .pImmutableSamplers = nullptr,
  };

  bindings.push_back(binding);

  return *this;
}

Result<DescriptorSetBuilder::DescriptorSet, Error> DescriptorSetBuilder::build() {
  auto layout = vk::DescriptorSetLayoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings    = bindings.data(),
  };
  return _dsl_manager->create_layout(layout).and_then([this](auto l) -> std::expected<DescriptorSet, Error> {
    return _allocator->allocate(l).transform([l](auto dsl) {
      return DescriptorSet{
          .descriptor_set = dsl,
          .layout         = l,
      };
    });
  });
}

Result<DescriptorSetBuilder::DescriptorSets, Error> DescriptorSetBuilder::build_many(size_t count) {
  auto layout = vk::DescriptorSetLayoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings    = bindings.data(),
  };

  return _dsl_manager->create_layout(layout).and_then([this, count](auto l) -> std::expected<DescriptorSets, Error> {
    return _allocator->allocate_many(l, count).transform([l](std::vector<vk::DescriptorSet>&& ds_vec) {
      return DescriptorSets{
          .descriptor_sets = std::move(ds_vec),
          .layout          = l,
      };
    });
  });
}

Result<vk::DescriptorSetLayout, Error> DescriptorSetBuilder::build_layout_only() {
  auto layout = vk::DescriptorSetLayoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings    = bindings.data(),
  };
  return _dsl_manager->create_layout(layout);
}

}  // namespace eray::vkren
