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

void DescriptorAllocator::clear_pools() {
  for (auto& p : ready_pools_) {
    p.reset();
  }
  for (auto&& p : full_pools_) {
    p.reset();
    ready_pools_.push_back(std::move(p));
  }
  full_pools_.clear();
}

void DescriptorAllocator::destroy_pools() {
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

Result<vk::raii::DescriptorPool, Error> DescriptorAllocator::create_pool(uint32_t set_count,
                                                                         std::span<PoolSizeRatio> pool_ratios) {
  auto pool_sizes = std::vector<vk::DescriptorPoolSize>();
  for (PoolSizeRatio ratio : pool_ratios) {
    pool_sizes.push_back(vk::DescriptorPoolSize{
        .type            = ratio.type,
        .descriptorCount = static_cast<uint32_t>(static_cast<float>(set_count) * ratio.ratio),
    });
  }

  auto create_info = vk::DescriptorPoolCreateInfo{
      .sType         = vk::StructureType::eDescriptorPoolCreateInfo,
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

Result<void, Error> DescriptorAllocator::init(uint32_t max_sets, std::span<PoolSizeRatio> pool_size_ratios) {
  ratios_.clear();

  for (auto r : pool_size_ratios) {
    ratios_.push_back(r);
  }

  auto new_pool = create_pool(max_sets, ratios_);
  if (!new_pool) {
    return std::unexpected(new_pool.error());
  }

  sets_per_pool_ = max_sets + max_sets / 2;  // grow it next allocation
  ready_pools_.emplace_back(std::move(*new_pool));

  return {};
}

Result<vk::DescriptorSet, Error> DescriptorAllocator::allocate(vk::DescriptorSetLayout layout, void* p_next) {
  auto pool_to_use = get_pool();
  if (!pool_to_use) {
    return std::unexpected(pool_to_use.error());
  }

  auto alloc_info = vk::DescriptorSetAllocateInfo{
      .sType              = vk::StructureType::eDescriptorSetAllocateInfo,
      .pNext              = p_next,
      .descriptorPool     = *pool_to_use,
      .descriptorSetCount = 1,
      .pSetLayouts        = &layout,
  };

  using Expected = std::expected<std::vector<vk::raii::DescriptorSet>, Error>;

  auto ds_opt = (*p_device_)->allocateDescriptorSets(alloc_info).or_else([&](auto&& err) -> Expected {
    if (err == vk::Result::eErrorOutOfPoolMemory || err == vk::Result::eErrorFragmentedPool) {
      return get_pool().and_then([&](vk::raii::DescriptorPool&& pool) -> Expected {
        pool_to_use               = std::move(pool);
        alloc_info.descriptorPool = *pool_to_use;

        return (*p_device_)->allocateDescriptorSets(alloc_info).or_else([](auto&& err) -> Expected {
          return std::unexpected(Error{
              .msg     = "Descriptor Sets creation failure",
              .code    = ErrorCode::VulkanObjectCreationFailure{},
              .vk_code = err,
          });
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
    return std::unexpected(ds_opt.error());
  }

  ready_pools_.push_back(std::move(*pool_to_use));

  return std::move(ds_opt->front());
}

void DescriptorWriter::write_sampler(uint32_t binding, vk::Sampler sampler) {
  write_image(binding, VK_NULL_HANDLE, sampler, vk::ImageLayout::eUndefined, vk::DescriptorType::eSampler);
}

void DescriptorWriter::write_sampled_image(uint32_t binding, vk::ImageView image, vk::ImageLayout layout) {
  write_image(binding, image, VK_NULL_HANDLE, layout, vk::DescriptorType::eSampledImage);
}

void DescriptorWriter::write_combined_image_sampler(uint32_t binding, vk::ImageView image, vk::Sampler sampler,
                                                    vk::ImageLayout layout) {
  write_image(binding, image, sampler, layout, vk::DescriptorType::eCombinedImageSampler);
}

void DescriptorWriter::write_storage_image(uint32_t binding, vk::ImageView image, vk::ImageLayout layout) {
  write_image(binding, image, VK_NULL_HANDLE, layout, vk::DescriptorType::eStorageImage);
}

void DescriptorWriter::write_image(uint32_t binding, vk::ImageView image, vk::Sampler sampler, vk::ImageLayout layout,
                                   vk::DescriptorType type) {
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

void DescriptorWriter::write_buffer(uint32_t binding, vk::DescriptorBufferInfo info, vk::DescriptorType type) {
  write_buffer(binding, info.buffer, info.range, info.offset, type);
}

void DescriptorWriter::write_buffer(uint32_t binding, vk::Buffer buffer, size_t size, size_t offset,
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

void DescriptorWriter::clear() {
  image_infos.clear();
  writes.clear();
  buffer_infos.clear();
}

void DescriptorWriter::update_set(vk::DescriptorSet set) {
  for (auto& write : writes) {
    write.dstSet = set;
  }

  (*_p_device)->updateDescriptorSets(writes, nullptr);
}

DescriptorWriter DescriptorWriter::create(Device& device) {
  return DescriptorWriter{
      .image_infos  = {},
      .buffer_infos = {},
      .writes       = {},
      ._p_device    = &device,
  };
}

}  // namespace eray::vkren
