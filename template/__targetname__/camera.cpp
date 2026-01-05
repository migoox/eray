
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>

#include <__targetname__/camera.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/os/input.hpp>
#include <liberay/os/window/input_codes.hpp>
#include <liberay/os/window/mouse_cursor_codes.hpp>
#include <liberay/util/logger.hpp>

namespace __namespace__ {

using Logger = eray::util::Logger;

namespace math = eray::math;
namespace os   = eray::os;

void Camera::on_process_physics(eray::os::InputManager& input_manager, float delta) {
  auto mouse_delta = math::Vec2f{input_manager.delta_mouse_pos_x(), input_manager.delta_mouse_pos_y()};

  bool modified = false;
  if (input_manager.is_mouse_btn_pressed(os::MouseBtnCode::MouseButtonLeft)) {
    yaw_ -= mouse_delta.x() * 0.4F * delta;
    pitch_ -= mouse_delta.y() * 0.4F * delta;
    pitch_ = std::clamp(pitch_, kMinPitch, kMaxPitch);

    modified = true;
  }

  if (input_manager.is_mouse_btn_pressed(os::MouseBtnCode::MouseButtonMiddle)) {
    auto basis_up    = math::Vec3f(math::rotation_y(yaw_) * math::rotation_x(pitch_) * math::Vec4f{0.F, 1.F, 0.F, 0.F});
    auto basis_right = math::Vec3f(math::rotation_y(yaw_) * math::rotation_x(pitch_) * math::Vec4f{1.F, 0.F, 0.F, 0.F});
    origin_ += (basis_up * mouse_delta.y() - basis_right * mouse_delta.x()) * 0.4F * delta;

    modified = true;
  }

  if (input_manager.just_scrolled()) {
    distance_ += input_manager.delta_mouse_scroll_y() * 14.F * delta;
    distance_ = std::clamp(distance_, 0.01F, 100.F);
  }

  if (modified) {
    recalculate();
  }
}

Camera::Camera(bool orthographic, float fov, float aspect_ratio, float near_plane, float far_plane)
    : is_orthographic_(orthographic),
      fov_(fov),
      aspect_ratio_(aspect_ratio),
      width_(),
      height_(),
      near_plane_(near_plane),
      far_plane_(far_plane),
      projection_(eray::math::Mat4f::identity()) {
  recalculate();
}

void Camera::set_orthographic(bool ortho) {
  is_orthographic_ = ortho;
  recalculate();
}

void Camera::set_fov(float fov) {
  fov_ = fov;
  recalculate();
}

void Camera::set_aspect_ratio(float aspect_ratio) {
  aspect_ratio_ = aspect_ratio;
  recalculate();
}

void Camera::set_near_plane(float near_plane) {
  near_plane_ = near_plane;
  recalculate();
}

void Camera::set_far_plane(float far_plane) { far_plane_ = far_plane; }

void Camera::recalculate() {
  float focal_length = is_orthographic_ ? distance_ : near_plane_;
  height_            = focal_length * std::tan(fov_ * 0.5F);
  width_             = height_ * aspect_ratio_;

  view_ = math::translation(math::Vec3f(0.F, 0.F, -distance_)) * math::rotation_x(-pitch_) * math::rotation_y(-yaw_) *
          math::translation(-origin_);
  inv_view_ = math::translation(origin_) * math::rotation_y(yaw_) * math::rotation_x(pitch_) *
              math::translation(math::Vec3f(0.F, 0.F, distance_));
  pos_ = math::Vec3f(inv_view_ * math::Vec4f(0.F, 0.F, 1.F, 1.F));

  if (is_orthographic_) {
    projection_     = eray::math::orthographic_vk_rh(-width_, width_, -height_, height_, near_plane_, far_plane_);
    inv_projection_ = eray::math::inv_orthographic_vk_rh(-width_, width_, -height_, height_, near_plane_, far_plane_);
  } else {
    projection_     = eray::math::perspective_vk_rh(fov_, aspect_ratio_, near_plane_, far_plane_);
    inv_projection_ = eray::math::inv_perspective_vk_rh(fov_, aspect_ratio_, near_plane_, far_plane_);
  }
}

void Camera::set_pitch(float pitch) {
  pitch_ = std::clamp(pitch, kMinPitch, kMaxPitch);
  recalculate();
}

void Camera::set_yaw(float yaw) {
  yaw_ = yaw;
  recalculate();
}

void Camera::set_distance_from_origin(float distance) {
  distance_ = distance;
  recalculate();
}

void Camera::set_origin(const eray::math::Vec3f& origin) {
  origin_ = origin;
  recalculate();
}

}  // namespace __namespace__
