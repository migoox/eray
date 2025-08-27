#pragma once

#include <liberay/math/vec.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

struct UniformBufferObject {
  float delta_time;
};

struct Particle {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;
  using Vec4 = eray::math::Vec4f;

  Vec2 position;
  Vec2 velocity;
  Vec4 color;
};

struct ParticleSystem {
  static constexpr size_t kParticleCount = 8192;

  static ParticleSystem create_on_circle(float aspect_ratio = 1.F);

  static vk::VertexInputBindingDescription get_binding_desc() {
    return vk::VertexInputBindingDescription{
        .binding   = 0,
        .stride    = sizeof(Particle),
        .inputRate = vk::VertexInputRate::eVertex,
    };
  }

  static auto get_attribs_desc() {
    return std::array{
        vk::VertexInputAttributeDescription{
            .location = 0,
            .binding  = 0,
            .format   = vk::Format::eR32G32Sfloat,
            .offset   = offsetof(Particle, position),
        },
        vk::VertexInputAttributeDescription{
            .location = 1,
            .binding  = 0,
            .format   = vk::Format::eR32G32B32A32Sfloat,
            .offset   = offsetof(Particle, color),
        },
    };
  }

  std::vector<Particle> particles;
};
