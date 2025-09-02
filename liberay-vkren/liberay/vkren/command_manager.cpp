#include <expected>
#include <liberay/vkren/command_manager.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "liberay/util/logger.hpp"

namespace eray::vkren {

Result<void, Error> CommandManager::allocate_command_buffers(const Device& device, uint32_t thread_count,
                                                             uint32_t buffers_per_thread) {
  std::lock_guard<std::mutex> lock(resource_mtx_);

  command_buffers_.clear();
  for (uint32_t i = 0; i < thread_count; ++i) {
    auto info = vk::CommandBufferAllocateInfo{
        .commandPool        = *command_pools_[i],
        .level              = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = buffers_per_thread,
    };

    if (auto thread_buffers = device->allocateCommandBuffers(info)) {
      for (auto& buffer : *thread_buffers) {
        command_buffers_.emplace_back(std::move(buffer));
      }
    } else {
      util::Logger::err("Could not allocate a command buffer");
      return std::unexpected(Error{
          .msg     = "Command Buffer allocation failure",
          .code    = ErrorCode::VulkanObjectCreationFailure{},
          .vk_code = thread_buffers.error(),
      });
    }
  }
  return {};
}

Result<void, Error> CommandManager::create_thread_command_pools(const Device& device, uint32_t queue_family_index,
                                                                uint32_t thread_count) {
  auto lock = std::lock_guard<std::mutex>(resource_mtx_);
  command_pools_.clear();
  for (auto i = 0U; i < thread_count; ++i) {
    auto info = vk::CommandPoolCreateInfo{
        .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queue_family_index,
    };
    if (auto result = device->createCommandPool(info)) {
      command_pools_.emplace_back(std::move(*result));
    } else {
      command_pools_.clear();
      util::Logger::err("Could not create a command pool");
      return std::unexpected(Error{
          .msg     = "Command Pool creation failed",
          .code    = ErrorCode::VulkanObjectCreationFailure{},
          .vk_code = result.error(),
      });
    }
  }

  return {};
}

vk::raii::CommandPool& CommandManager::get_command_pool(uint32_t thread_index) {
  auto lock = std::lock_guard<std::mutex>(resource_mtx_);
  return command_pools_[thread_index];
}

vk::raii::CommandBuffer& CommandManager::get_command_buffer(uint32_t thread_index) {
  auto buffer = std::lock_guard<std::mutex>(resource_mtx_);
  return command_buffers_[thread_index];
}

}  // namespace eray::vkren
