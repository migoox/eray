#pragma once

#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

class CommandManager {
 public:
  Result<void, Error> create_thread_command_pools(const Device& device, uint32_t queue_family_index,
                                                  uint32_t thread_count);
  vk::raii::CommandPool& get_command_pool(uint32_t thread_index);

  Result<void, Error> allocate_command_buffers(const Device& device, uint32_t thread_count,
                                               uint32_t buffers_per_thread);

  vk::raii::CommandBuffer& get_command_buffer(uint32_t thread_index);

 private:
  std::mutex resource_mtx_;
  std::vector<vk::raii::CommandPool> command_pools_;
  std::vector<vk::raii::CommandBuffer> command_buffers_;
};

}  // namespace eray::vkren
