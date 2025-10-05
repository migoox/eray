#pragma once

#include <variant>

namespace eray::vkren {

struct OrthographicCamera {
  /**
   * @brief Vertical size of the orthographic view volume measured in world units
   *
   * left = -1/2*size*aspect
   * right = +1/2*size*aspect
   * bottom = -1/2*size
   * top = +1/2*size
   *
   */
  float size;
};

struct PerspectiveCamera {
  /**
   * @brief FOV angle in radians.
   *
   */
  float fov{};

  /**
   * @brief The FOV might be horizontal (false) or vertical (true).
   *
   */
  bool horizontal{false};
};

struct FrustumCamera {
  float left;
  float right;
  float bottom;
  float top;
};

struct Camera {
  float aspect_ratio{1.0F};

  float near_plane;
  float far_plane;

  std::variant<OrthographicCamera, PerspectiveCamera, FrustumCamera> projection;
};

}  // namespace eray::vkren
