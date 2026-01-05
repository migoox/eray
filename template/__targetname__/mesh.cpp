#include <__targetname__/mesh.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/vkren/buffer.hpp>

namespace __namespace__ {
namespace math  = eray::math;
namespace vkren = eray::vkren;

Mesh Mesh::create_box(eray::vkren::Device& device, eray::math::Vec3f color, const eray::math::Mat4f& mat) {
  struct Vertex {
    math::Vec3f pos;
    math::Vec3f normal;
    math::Vec3f color;
  };

  auto attribs_desc = std::vector{
      vk::VertexInputAttributeDescription{
          .location = 0,
          .binding  = 0,
          .format   = vk::Format::eR32G32B32Sfloat,
          .offset   = offsetof(Vertex, pos),
      },
      vk::VertexInputAttributeDescription{
          .location = 1,
          .binding  = 0,
          .format   = vk::Format::eR32G32B32Sfloat,
          .offset   = offsetof(Vertex, normal),
      },
      vk::VertexInputAttributeDescription{
          .location = 2,
          .binding  = 0,
          .format   = vk::Format::eR32G32B32Sfloat,
          .offset   = offsetof(Vertex, color),
      },
  };

  auto vb = std::vector<Vertex>{
      // TOP (+Y)
      Vertex{
          .pos    = math::Vec3f{0.5F, 0.5F, -0.5F},
          .normal = math::Vec3f{0.F, 1.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{0.5F, 0.5F, 0.5F},
          .normal = math::Vec3f{0.F, 1.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, 0.5F, 0.5F},
          .normal = math::Vec3f{0.F, 1.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, 0.5F, -0.5F},
          .normal = math::Vec3f{0.F, 1.F, 0.F},
          .color  = color,
      },

      // BOTTOM (−Y)
      Vertex{
          .pos    = math::Vec3f{0.5F, -0.5F, 0.5F},
          .normal = math::Vec3f{0.F, -1.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{0.5F, -0.5F, -0.5F},
          .normal = math::Vec3f{0.F, -1.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, -0.5F, -0.5F},
          .normal = math::Vec3f{0.F, -1.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, -0.5F, 0.5F},
          .normal = math::Vec3f{0.F, -1.F, 0.F},
          .color  = color,
      },

      // FRONT (+Z)
      Vertex{
          .pos    = math::Vec3f{0.5F, 0.5F, 0.5F},
          .normal = math::Vec3f{0.F, 0.F, 1.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{0.5F, -0.5F, 0.5F},
          .normal = math::Vec3f{0.F, 0.F, 1.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, -0.5F, 0.5F},
          .normal = math::Vec3f{0.F, 0.F, 1.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, 0.5F, 0.5F},
          .normal = math::Vec3f{0.F, 0.F, 1.F},
          .color  = color,
      },

      // BACK (−Z)
      Vertex{
          .pos    = math::Vec3f{-0.5F, 0.5F, -0.5F},
          .normal = math::Vec3f{0.F, 0.F, -1.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, -0.5F, -0.5F},
          .normal = math::Vec3f{0.F, 0.F, -1.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{0.5F, -0.5F, -0.5F},
          .normal = math::Vec3f{0.F, 0.F, -1.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{0.5F, 0.5F, -0.5F},
          .normal = math::Vec3f{0.F, 0.F, -1.F},
          .color  = color,
      },

      // LEFT (−X)
      Vertex{
          .pos    = math::Vec3f{-0.5F, 0.5F, 0.5F},
          .normal = math::Vec3f{-1.F, 0.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, -0.5F, 0.5F},
          .normal = math::Vec3f{-1.F, 0.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, -0.5F, -0.5F},
          .normal = math::Vec3f{-1.F, 0.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{-0.5F, 0.5F, -0.5F},
          .normal = math::Vec3f{-1.F, 0.F, 0.F},
          .color  = color,
      },

      // RIGHT (+X)
      Vertex{
          .pos    = math::Vec3f{0.5F, 0.5F, -0.5F},
          .normal = math::Vec3f{1.F, 0.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{0.5F, -0.5F, -0.5F},
          .normal = math::Vec3f{1.F, 0.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{0.5F, -0.5F, 0.5F},
          .normal = math::Vec3f{1.F, 0.F, 0.F},
          .color  = color,
      },
      Vertex{
          .pos    = math::Vec3f{0.5F, 0.5F, 0.5F},
          .normal = math::Vec3f{1.F, 0.F, 0.F},
          .color  = color,
      },
  };

  for (auto& v : vb) {
    v.pos   = math::Vec3f{mat * math::Vec4f{v.pos, 1.F}};
    v.color = color;
  }

  auto ib = std::vector<uint16_t>{
      0,  1,  2,  2,  3,  0,   // TOP
      4,  5,  6,  6,  7,  4,   // BOTTOM
      8,  9,  10, 10, 11, 8,   // FRONT
      12, 13, 14, 14, 15, 12,  // BACK
      16, 17, 18, 18, 19, 16,  // LEFT
      20, 21, 22, 22, 23, 20   // RIGHT
  };

  auto vertices_region = eray::util::MemoryRegion{vb.data(), sizeof(Vertex) * vb.size()};
  auto indices_region  = eray::util::MemoryRegion{ib.data(), sizeof(uint16_t) * ib.size()};

  auto vertex_buff = vkren::BufferResource::create_vertex_buffer(device, vertices_region.size_bytes())
                         .or_panic("Could not create the vertex buffer");
  vertex_buff.write(vertices_region).or_panic("Could not write to vertex buffer");

  auto index_buff = vkren::BufferResource::create_index_buffer(device, indices_region.size_bytes())
                        .or_panic("Could not create the index buffer");
  index_buff.write(indices_region).or_panic("Could not write to index buffer");

  return Mesh{
      .binding_desc =
          vk::VertexInputBindingDescription{
              .binding   = 0,
              .stride    = sizeof(Vertex),
              .inputRate = vk::VertexInputRate::eVertex,
          },
      .attribs_desc = std::move(attribs_desc),
      .vert_buffer  = std::move(vertex_buff),
      .ind_buffer   = std::move(index_buff),
      .ind_count    = static_cast<uint32_t>(ib.size()),
  };
}

void Mesh::render(vk::CommandBuffer graphics_command_buffer, uint32_t instance_count) const {
  graphics_command_buffer.bindVertexBuffers(0, vert_buffer.vk_buffer(), {0});
  graphics_command_buffer.bindIndexBuffer(ind_buffer.vk_buffer(), 0, vk::IndexType::eUint16);
  graphics_command_buffer.drawIndexed(ind_count, instance_count, 0, 0, 0);
}

}  // namespace __namespace__
