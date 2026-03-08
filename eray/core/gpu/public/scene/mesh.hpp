#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace eray::vkren {

struct GPUMeshSurface {
  vk::Buffer vertex_buffer;
  vk::Buffer index_buffer;
  uint32_t first_index;
  uint32_t index_count;
};

}  // namespace eray::vkren
