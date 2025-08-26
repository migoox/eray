#pragma once

#include <liberay/math/vec.hpp>
#include <vector>

struct Particle {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;
  using Vec4 = eray::math::Vec4f;

  Vec2 position;
  Vec2 velocity;
  Vec4 color;
};

struct ParticleSystem {
  static constexpr size_t kParticleCount = 1000;

  static ParticleSystem create_on_circle(float aspect_ratio = 1.F);

  std::vector<Particle> particles;
};
