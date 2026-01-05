#pragma once

#include <liberay/math/mat.hpp>
#include <liberay/math/transform3.hpp>
#include <liberay/os/input.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/window.hpp>

namespace __namespace__ {

class Camera {
 public:
  Camera(bool orthographic, float fov, float aspect_ratio, float near_plane, float far_plane);

  void on_process_physics(eray::os::InputManager& input_manager, float delta);

  bool is_orthographic() const { return is_orthographic_; }
  float fov() const { return fov_; }
  float aspect_ratio() const { return aspect_ratio_; }
  float near_plane() const { return near_plane_; }
  float far_plane() const { return far_plane_; }
  float width() const { return width_; }
  float height() const { return height_; }
  const eray::math::Vec3f& pos() const { return pos_; }

  void set_pitch(float pitch);
  void set_yaw(float yaw);
  void set_distance_from_origin(float distance);
  void set_origin(const eray::math::Vec3f& origin);

  void set_orthographic(bool ortho);
  void set_fov(float fov);
  void set_aspect_ratio(float aspect_ratio);
  void set_near_plane(float near_plane);
  void set_far_plane(float far_plane);

  const eray::math::Mat4f& view_matrix() const { return view_; }
  const eray::math::Mat4f& proj_matrix() const { return projection_; }
  const eray::math::Mat4f& inv_view_matrix() const { return inv_view_; }
  const eray::math::Mat4f& inv_proj_matrix() const { return inv_projection_; }

  void recalculate();

  static float constexpr kMinPitch = -std::numbers::pi_v<float> / 2.F;
  static float constexpr kMaxPitch = std::numbers::pi_v<float> / 2.F;

 private:
  bool is_orthographic_;

  float fov_;  // only apparent if orthographic
  float aspect_ratio_;
  float width_, height_;
  float near_plane_, far_plane_;

  eray::math::Mat4f projection_, inv_projection_, view_, inv_view_;
  eray::math::Vec3f pos_;
  eray::math::Vec3f origin_;

  float pitch_{0.F};
  float yaw_{0.F};
  float distance_{4.F};

  eray::math::Vec2f old_mouse_pos_;
};

}  // namespace __namespace__
