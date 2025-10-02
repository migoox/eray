#pragma once

#include <liberay/math/vec.hpp>
#include <variant>

namespace eray::vkren {

struct DirectionalLight {
  math::Vec3f direction;
};

struct PointLight {
  math::Vec3f position;

  float constant{1.0F};
  float linear{0.0F};
  float quadratic{0.0F};
};

struct SpotLight {
  math::Vec3f position;
  math::Vec3f direction;

  float inner_cutoff;  // radians
  float outer_cutoff;  // radians

  float constant{1.0F};
  float linear{0.0F};
  float quadratic{0.0F};
};

struct Light {
  math::Vec3f color;
  std::variant<DirectionalLight, PointLight, SpotLight> lights;
};

}  // namespace eray::vkren
