#pragma once

#include <liberay/math/vec.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

struct Vertex {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;

  Vec2 pos;
  Vec3 color;

  static vk::VertexInputBindingDescription get_binding_desc() {
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

  static std::array<vk::VertexInputAttributeDescription, 2> get_attribs_desc() {
    return {
        vk::VertexInputAttributeDescription{
            // References the location directive of the input in the vertex shader
            .location = 0,

            // From which binding the per-vertex data comes.
            .binding = 0,

            // Describes the type of data for the attribute
            .format = vk::Format::eR32G32Sfloat,

            .offset = offsetof(Vertex, pos),
        },
        vk::VertexInputAttributeDescription{
            .location = 1,
            .binding  = 0,
            .format   = vk::Format::eR32G32B32Sfloat,
            .offset   = offsetof(Vertex, color),
        },

    };
  }
};

struct VertexBuffer {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;

  static VertexBuffer create_triangle() {
    // interleaving vertex attributes
    return VertexBuffer{.vertices =
                            std::vector<Vertex>{Vertex{.pos = Vec2{0.0F, -0.5F}, .color = Vec3{1.0F, 0.0F, 0.0F}},
                                                Vertex{.pos = Vec2{0.5F, 0.5F}, .color = Vec3{0.0F, 1.0F, 0.0F}},
                                                Vertex{.pos = Vec2{-0.5F, 0.5F}, .color = Vec3{0.0F, 0.0F, 1.0F}}}};
  }

  vk::BufferCreateInfo get_create_info(vk::SharingMode sharing_mode) const {
    return vk::BufferCreateInfo{
        // Flags configure sparse buffer memory
        .flags = {},

        // Specifies size of the buffer in bytes
        .size = size_in_bytes(),

        .usage = vk::BufferUsageFlagBits::eVertexBuffer,

        // Just like the images in the swap chain, buffers might also be owned by a specific queue family or be shared
        // between multiple at the same time
        .sharingMode = sharing_mode,
    };
  }

  uint32_t size_in_bytes() const { return static_cast<uint32_t>(sizeof(Vertex) * vertices.size()); }

  std::vector<Vertex> vertices;
};
