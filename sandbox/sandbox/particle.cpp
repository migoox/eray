#include <liberay/math/color.hpp>
#include <numbers>
#include <random>
#include <sandbox/particle.hpp>

ParticleSystem ParticleSystem::create_on_circle(float aspect_ratio) {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;
  using Vec4 = eray::math::Vec4f;

  auto rnd_engine = std::default_random_engine(static_cast<unsigned>(std::time(nullptr)));
  auto rnd_dist   = std::uniform_real_distribution<float>(0.F, 1.F);

  auto result = std::vector<Particle>(kParticleCount);
  for (auto& particle : result) {
    const auto radius = rnd_dist(rnd_engine) * 0.25F;
    const auto theta  = rnd_dist(rnd_engine) * 2.F * std::numbers::pi_v<float>;
    particle.position = Vec2(radius * std::cos(theta) * 1.F / aspect_ratio, radius * std::sin(theta));
    particle.color    = Vec4(eray::math::hsv2rgb(Vec3(rnd_dist(rnd_engine), 0.5F + 0.5F * rnd_dist(rnd_engine),
                                                      0.5F + 0.5F * rnd_dist(rnd_engine))),
                             1.F);
    particle.velocity = normalize(particle.position) * 0.00025F;
  }

  return ParticleSystem{
      .particles = result,
  };
}
