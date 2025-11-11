#pragma once
#include <liberay/math/quat.hpp>
#include <liberay/math/transform3_fwd.hpp>
#include <optional>
#include <vector>

namespace eray::math {

template <CFloatingPoint T>
struct Transform3 final {
 public:
  explicit Transform3(const Vec3<T> pos   = Vec3<T>{static_cast<T>(0), static_cast<T>(0), static_cast<T>(0)},
                      const Quat<T> rot   = Quat<T>{static_cast<T>(1), static_cast<T>(0), static_cast<T>(0),
                                                    static_cast<T>(0)},
                      const Vec3<T> scale = Vec3<T>{static_cast<T>(1), static_cast<T>(1), static_cast<T>(1)})
      : pos_(pos), rot_(rot), scale_(scale), dirty_(true), inv_dirty_(true) {}

  ~Transform3() {
    for (const auto child : children_) {
      child.get().parent_.reset();
    }

    if (parent_.has_value()) {
      remove_parent();
    }
  }

  Transform3(const Transform3&)            = delete;
  Transform3& operator=(const Transform3&) = delete;

  Transform3(Transform3&& other) noexcept
      : parent_(std::move(other.parent_)),
        children_(std::move(other.children_)),
        pos_(std::move(other.pos_)),
        rot_(std::move(other.rot_)),
        scale_(std::move(other.scale_)),
        dirty_(other.dirty_),
        model_mat_(std::move(other.model_mat_)),
        inv_dirty_(other.inv_dirty_),
        inv_model_mat_(std::move(other.inv_model_mat_)) {
    for (auto& child : children_) {
      child.get().parent_ = *this;
    }

    if (parent_.has_value()) {
      auto& parent_children = parent_->get().children_;
      auto it               = std::find_if(parent_children.begin(), parent_children.end(),
                                           [&other](const auto& ref) { return &ref.get() == &other; });
      if (it != parent_children.end()) {
        *it = *this;
      }
    }

    other.parent_.reset();
    other.children_.clear();
  }

  Transform3& operator=(Transform3&& other) noexcept {
    if (this != &other) {
      if (parent_.has_value()) {
        remove_parent();
      }
      for (const auto& child : children_) {
        child.get().parent_.reset();
      }

      parent_        = std::move(other.parent_);
      children_      = std::move(other.children_);
      pos_           = std::move(other.pos_);
      rot_           = std::move(other.rot_);
      scale_         = std::move(other.scale_);
      dirty_         = other.dirty_;
      model_mat_     = std::move(other.model_mat_);
      inv_dirty_     = other.inv_dirty_;
      inv_model_mat_ = std::move(other.inv_model_mat_);

      for (auto& child : children_) {
        child.get().parent_ = *this;
      }

      if (parent_.has_value()) {
        auto& parent_children = parent_->get().children_;
        auto it               = std::find_if(parent_children.begin(), parent_children.end(),
                                             [&other](const auto& ref) { return &ref.get() == &other; });
        if (it != parent_children.end()) {
          *it = *this;
        }
      }

      other.parent_.reset();
      other.children_.clear();
    }
    return *this;
  }

  bool has_parent() const { return parent_.has_value(); }

  const Transform3& parent() const { return *parent_; }

  void reset_local(const Vec3<T> pos   = Vec3<T>{static_cast<T>(0), static_cast<T>(0), static_cast<T>(0)},
                   const Quat<T> rot   = Quat<T>{static_cast<T>(1), static_cast<T>(0), static_cast<T>(0),
                                                 static_cast<T>(0)},
                   const Vec3<T> scale = Vec3<T>{static_cast<T>(1), static_cast<T>(1), static_cast<T>(1)}) {
    pos_   = pos;
    rot_   = rot;
    scale_ = scale;
    mark_dirty();
  }

  /**
   * @brief Set parent. Treat the current local transform as a transform in parent's coordinates.
   *
   * @param parent
   */
  void local_set_parent(Transform3& parent) {
    if (parent_) {
      remove_parent();
    }
    parent_ = parent;
    parent_->get().children_.emplace_back(*this);
    mark_dirty();
  }

  /**
   * @brief Set parent while preserving the world transform.
   *
   * @param parent
   */
  void set_parent(Transform3& parent) {
    if (parent_) {
      remove_parent();
    }

    auto new_pos   = pos() - parent.pos();
    auto new_rot   = math::normalize(parent.rot().conjugated() * rot());
    auto new_scale = 1.F / parent.scale() * scale();
    parent_        = parent;
    parent_->get().children_.emplace_back(*this);
    reset_local(new_pos, new_rot, new_scale);
    mark_dirty();
  }

  /**
   * @brief Updates the local positions by a displacement along world axises.
   *
   * @param delta
   */
  void move(const Vec3<T>& delta) {
    pos_ += delta;
    mark_dirty();
  }

  /**
   * @brief Updates the local positions by a displacement along its local axises.
   *
   * @param delta
   */
  void move_local(const Vec3<T>& delta) {
    pos_ += delta.x * local_right() + delta.y * local_up() + delta.z * local_front();
    mark_dirty();
  }

  /**
   * @brief Updates the local rotation around assuming the axis is expressed in world coordinate system.
   *
   * @param angle, axis
   */
  void rotate(float angle, const Vec3<T>& axis) {
    rot_ = normalize(rot_ * Quat<T>::rotation_axis(angle, axis));
    mark_dirty();
  }

  void rotate(const Quat<T>& rotation) {
    rot_ = normalize(rot_ * rotation);
    mark_dirty();
  }

  /**
   * @brief Updates the local rotation around assuming the axis (encoded in quaternion) is expressed in local
   * coordinate system.
   *
   * @param rotation
   */
  void rotate_local(const Quat<T>& rotation) {
    rot_ = normalize(rotation * rot_);
    mark_dirty();
  }

  const Vec3<T>& local_pos() const { return pos_; }

  Vec3<T>& local_pos() { return pos_; }

  Vec3<T> pos() const {
    return parent_.has_value()
               ? Vec3<T>(parent().local_to_world_matrix() * Vec4<T>(pos_.x(), pos_.y(), pos_.z(), static_cast<T>(1)))
               : pos_;
  }

  void set_local_pos(const Vec3<T>& pos) {
    pos_ = pos;
    mark_dirty();
  }

  const Quat<T>& local_rot() const { return rot_; }

  Quat<T>& local_rot() { return rot_; }

  Quat<T> rot() const { return parent_.has_value() ? parent().rot() * rot_ : rot_; }

  void set_local_rot(const Quat<T>& rot) {
    rot_ = rot;
    mark_dirty();
  }

  const Vec3<T>& local_scale() const { return scale_; }

  Vec3<T>& local_scale() { return scale_; }

  Vec3<T> scale() const { return parent_.has_value() ? parent().scale() * scale_ : scale_; }

  void set_local_scale(Vec3<T> scale) {
    scale_ = scale;
    mark_dirty();
  }

  // TODO(migoox): Implement the matrix decomposition
  // void set_local_from_matrix(const Mat4<T>& local_mat);

  Vec3<T> local_front() const { return rot_ * Vec3<T>(0, 0, -1); }

  Vec3<T> front() const {
    const Vec3<T> local = local_front();
    return parent_
               ? Vec3<T>(normalize(parent().local_to_world_matrix() * Vec4<T>(local.x(), local.y(), local.z(), 0.0F)))
               : local;
  }

  Vec3<T> local_right() const { return rot_ * Vec3<T>(1, 0, 0); }

  Vec3<T> right() const {
    const Vec3<T> local = local_right();
    return parent_
               ? Vec3<T>(normalize(parent().local_to_world_matrix() * Vec4<T>(local.x(), local.y(), local.z(), 0.0F)))
               : local;
  }

  Vec3<T> local_up() const { return rot_ * Vec3<T>(0, 1, 0); }

  Vec3<T> up() const {
    const Vec3<T> local = local_up();
    return parent_
               ? Vec3<T>(normalize(parent().local_to_world_matrix() * Vec4<T>(local.x(), local.y(), local.z(), 0.0F)))
               : local;
  }

  Mat3<T> local_orientation() const {
    Mat3<T> local = rot_mat3_from_quat(rot_);
    return Mat3<T>{local[0], local[1], -local[2]};
  }

  Mat3<T> orientation() const {
    const Mat3 local = local_orientation();
    if (!parent_) {
      return local;
    }
    const Mat3 orientation =
        Mat3<T>(Vec3<T>(parent().local_to_world_matrix()[0]), Vec3<T>(parent().local_to_world_matrix()[1]),
                Vec3<T>(parent().local_to_world_matrix()[2])) *
        local;
    return Mat3<T>{normalize(orientation[0]), normalize(orientation[1]), normalize(orientation[2])};
  }

  void mark_dirty() const {
    if (dirty_ && inv_dirty_) {
      return;
    }

    dirty_ = inv_dirty_ = true;
    for (const auto child : children_) {
      child.get().mark_dirty();
    }
  }

  Mat4<T> local_to_parent_matrix() const {
    return translation(pos_) * rot_mat_from_quat(rot_) * ::eray::math::scale(scale_);
  }

  Mat4<T> parent_to_local_matrix() const {
    return ::eray::math::scale(Vec3<T>(scale_.x() < 1e-6F ? 0.0F : 1.0F / scale_.x(),  //
                                       scale_.y() < 1e-6F ? 0.0F : 1.0F / scale_.y(),  //
                                       scale_.z() < 1e-6F ? 0.0F : 1.0F / scale_.z())) *
           rot_mat_from_quat(conjugate(rot_)) * translation(-pos_);
  }

  const Mat4<T>& local_to_world_matrix() const {
    if (!dirty_) {
      return model_mat_;
    }

    model_mat_ = local_to_parent_matrix();
    if (parent_) {
      model_mat_ = parent_->get().local_to_world_matrix() * model_mat_;
    }

    dirty_ = false;
    return model_mat_;
  }

  const Mat4<T>& world_to_local_matrix() const {
    if (!inv_dirty_) {
      return inv_model_mat_;
    }

    inv_model_mat_ = parent_to_local_matrix();
    if (parent_) {
      inv_model_mat_ *= parent_->get().world_to_local_matrix();
    }

    inv_dirty_ = false;
    return inv_model_mat_;
  }

  // TODO(migoox): add normal matrix function
  /**
   * @brief Detaches from parent if the parent exists. After detachment the transform will be set to the
   * local transform.
   *
   */
  void local_detach_from_parent() {
    if (parent_) {
      remove_parent();
      mark_dirty();
    }
  }

  /**
   * @brief Detaches from parent if the parent exists. Updates the local transform to match the world transform
   * inherited from parent.
   *
   */
  void detach_from_parent() {
    if (!parent_) {
      return;
    }

    auto new_pos   = Vec3<T>(parent().local_to_world_matrix() * Vec4<T>(pos_, static_cast<T>(1)));
    auto new_rot   = Quat<T>(parent().rot() * rot_);
    auto new_scale = Vec3<T>(parent().scale() * scale_);
    remove_parent();
    reset_local(new_pos, new_rot, new_scale);
    mark_dirty();
  }

  /**
   * @brief Creates a copy of transform detached from parent (if exists) and without children. Sets the local
   * transform to match the world transform of the cloned object.
   *
   * @return Transform3
   */
  Transform3 clone_detached() const {
    if (parent_) {
      auto new_pos   = Vec3<T>(parent().local_to_world_matrix() * Vec4<T>(pos_, static_cast<T>(1)));
      auto new_rot   = Quat<T>(parent().rot() * rot_);
      auto new_scale = Vec3<T>(parent().scale() * scale_);

      return Transform3(new_pos, new_rot, new_scale);
    }

    return Transform3(pos_, rot_, scale_);
  }

 private:
  void remove_parent() {
    std::erase_if(parent_->get().children_, [this](auto ref) { return std::addressof(ref.get()) == this; });
    parent_.reset();
  }

 private:
  std::optional<std::reference_wrapper<Transform3>> parent_;
  std::vector<std::reference_wrapper<Transform3>> children_{};

  Vec3<T> pos_;
  Quat<T> rot_;
  Vec3<T> scale_;

  mutable bool dirty_            = false;
  mutable Mat4<T> model_mat_     = Mat4<T>::identity();
  mutable bool inv_dirty_        = false;
  mutable Mat4<T> inv_model_mat_ = Mat4<T>::identity();

};  // class Transform

}  // namespace eray::math
