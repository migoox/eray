#pragma once

#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/vkren/buffer.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace __namespace__ {

struct Mesh {
  vk::VertexInputBindingDescription binding_desc;
  std::vector<vk::VertexInputAttributeDescription> attribs_desc;
  eray::vkren::BufferResource vert_buffer;
  eray::vkren::BufferResource ind_buffer;
  uint32_t ind_count;

  static Mesh create_box(eray::vkren::Device& device, eray::math::Vec3f color, const eray::math::Mat4f& mat);

  void render(vk::CommandBuffer graphics_command_buffer, uint32_t instance_count = 1) const;
};

}  // namespace __namespace__
