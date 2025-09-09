#pragma once

#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

struct Vertex {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;

  Vec3 pos;
  Vec3 color;
  Vec2 tex_coord;

  static vk::VertexInputBindingDescription binding_desc() {
    return vk::VertexInputBindingDescription{
        // Index of the binding in the array of bindings
        .binding = 0,

        // Specifies the number of bytes from one entry to the next
        .stride = sizeof(Vertex),

        // VK_VERTEX_INPUT_RATE_VERTEX: move to the next data entry after each vertex
        // VK_VERTEX_INPUT_RATE_INSTANCE: move to the next data entry after each instance (instanced rendering)
        .inputRate = vk::VertexInputRate::eVertex,
    };
  }

  static auto attribs_desc() {
    return std::array{
        vk::VertexInputAttributeDescription{
            // References the location directive of the input in the vertex shader
            .location = 0,

            // From which binding the per-vertex data comes.
            .binding = 0,

            // Describes the type of data for the attribute
            .format = vk::Format::eR32G32B32Sfloat,

            .offset = offsetof(Vertex, pos),
        },
        vk::VertexInputAttributeDescription{
            .location = 1,
            .binding  = 0,
            .format   = vk::Format::eR32G32B32Sfloat,
            .offset   = offsetof(Vertex, color),
        },
        vk::VertexInputAttributeDescription{
            .location = 2,
            .binding  = 0,
            .format   = vk::Format::eR32G32Sfloat,
            .offset   = offsetof(Vertex, tex_coord),
        },
    };
  }
};

struct VertexBuffer {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;

  static VertexBuffer create() {
    // interleaving vertex attributes
    return VertexBuffer{
        .vertices =
            std::vector<Vertex>{
                Vertex{.pos = Vec3{0.5F, 0.5F, 0.F}, .color = Vec3{1.0F, 0.0F, 0.0F}, .tex_coord = Vec2{1.F, 1.F}},
                Vertex{.pos = Vec3{0.5F, -0.5F, 0.F}, .color = Vec3{0.0F, 1.0F, 0.0F}, .tex_coord = Vec2{1.F, 0.F}},
                Vertex{.pos = Vec3{-0.5F, -0.5F, 0.F}, .color = Vec3{0.0F, 0.0F, 1.0F}, .tex_coord = Vec2{0.F, 0.F}},
                Vertex{.pos = Vec3{-0.5F, 0.5F, 0.F}, .color = Vec3{1.0F, 0.0F, 0.0F}, .tex_coord = Vec2{0.F, 1.F}},

                Vertex{.pos = Vec3{0.5F, 0.5F, 0.5F}, .color = Vec3{1.0F, 1.0F, 0.0F}, .tex_coord = Vec2{1.F, 1.F}},
                Vertex{.pos = Vec3{0.5F, -0.5F, 0.5F}, .color = Vec3{0.0F, 1.0F, 1.0F}, .tex_coord = Vec2{1.F, 0.F}},
                Vertex{.pos = Vec3{-0.5F, -0.5F, 0.5F}, .color = Vec3{0.0F, 0.0F, 1.0F}, .tex_coord = Vec2{0.F, 0.F}},
                Vertex{.pos = Vec3{-0.5F, 0.5F, 0.5F}, .color = Vec3{1.0F, 0.0F, 1.0F}, .tex_coord = Vec2{0.F, 1.F}}},
        .indices =
            std::vector<uint16_t>{
                4, 5, 6,  //
                6, 7, 4,  //
                0, 1, 2,  //
                2, 3, 0,  //
            },
    };
  }

  vk::BufferCreateInfo create_info(vk::SharingMode sharing_mode) const {
    return vk::BufferCreateInfo{
        // Flags configure sparse buffer memory
        .flags = {},

        // Specifies size of the buffer in bytes
        .size = vertices_size_bytes(),

        .usage = vk::BufferUsageFlagBits::eVertexBuffer,

        // Just like the images in the swap chain, buffers might also be owned by a specific queue family or be shared
        // between multiple at the same time
        .sharingMode = sharing_mode,
    };
  }

  uint32_t vertices_size_bytes() const { return static_cast<uint32_t>(sizeof(Vertex) * vertices.size()); }
  uint32_t indices_size_bytes() const { return static_cast<uint32_t>(sizeof(uint16_t) * indices.size()); }

  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;
};

struct UniformBufferObject {
  using Mat4 = eray::math::Mat4f;

  // A float4x4 matrix must have the same alignment as a float4
  alignas(16) Mat4 model;
  alignas(16) Mat4 view;
  alignas(16) Mat4 proj;
};
