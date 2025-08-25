#pragma once

#include <liberay/math/vec.hpp>
#include <vector>

struct Particle {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;
  using Vec4 = eray::math::Vec3f;

  Vec2 position;
  Vec2 velocity;
  Vec4 color;
};

struct ParticleSystem {
  static ParticleSystem create() { return {.particles = {Particle{}}}; }

  std::vector<Particle> particles;
};
