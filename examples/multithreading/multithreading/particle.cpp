#include <liberay/math/color.hpp>
#include <liberay/math/vec.hpp>
#include <multithreading/particle.hpp>
#include <numbers>
#include <random>

ParticleSystem ParticleSystem::create_on_circle(float aspect_ratio) {
  using Vec2 = eray::math::Vec2f;
  using Vec4 = eray::math::Vec4f;

  auto rnd_engine = std::default_random_engine(static_cast<unsigned>(std::time(nullptr)));
  auto rnd_dist   = std::uniform_real_distribution<float>(0.F, 1.F);

  auto result = std::vector<Particle>(kParticleCount);
  for (auto& particle : result) {
    const auto radius = rnd_dist(rnd_engine) * 0.25F;
    const auto theta  = rnd_dist(rnd_engine) * 2.F * std::numbers::pi_v<float>;
    particle.position = Vec2(radius * std::cos(theta) * 1.F / aspect_ratio, radius * std::sin(theta));
    particle.velocity = eray::math::normalize(particle.position) * 0.25F;
    particle.color    = Vec4(rnd_dist(rnd_engine), rnd_dist(rnd_engine), rnd_dist(rnd_engine), 1.F);
  }

  return ParticleSystem{
      .particles = result,
  };
}
