#include <gtest/gtest.h>

#include <liberay/math/transform3.hpp>
#include <liberay/math/vec.hpp>
#include <numbers>
#include <tests/helpers/math_helpers.hpp>

constexpr float kPi = std::numbers::pi_v<float>;

using namespace eray::math;  // NOLINT

class TransformTest : public testing::Test {
 protected:
  TransformTest() : parent_transform_(Vec3f(1.F, 2.F, 3.F), Quatf(Quatf::rotation_y(kPi / 2.F)), Vec3f::filled(2.F)) {
    transform_.set_parent(parent_transform_);
  }

  // at:       1,2,3
  // rotation: 90deg around Y axis
  // scale:    2
  Transform3f parent_transform_;

  // at:       0,0,0
  // rotation: none
  // scale:    1
  Transform3f transform_;
};

/**
 * Defaults
 */

TEST_F(TransformTest, DefaultConstructorWorks) {
  // given / when
  const Transform3f transform;

  // then
  EXPECT_VEC_NEAR(transform.local_pos(), Vec3f(0.F, 0.F, 0.F), 1e-5F);
  EXPECT_VEC_NEAR(transform.local_rot().imaginary(), Vec3f(0.F, 0.F, 0.F), 1e-5F);
  EXPECT_EQ(transform.local_rot().real(), 1);
  EXPECT_VEC_NEAR(transform.local_scale(), Vec3f(1.F, 1.F, 1.F), 1e-5F);
}

TEST_F(TransformTest, ParametricConstructorWorks) {
  // given
  const auto pos   = Vec3f(1.F, 2.F, 3.F);
  const auto rot   = Quatf(4, 5, 6, 7);
  const auto scale = Vec3f::filled(8.F);

  // when
  const Transform3f transform(pos, rot, scale);

  // then
  EXPECT_VEC_NEAR(transform.local_pos(), pos, 1e-5F);
  EXPECT_VEC_NEAR(transform.local_rot().imaginary(), rot.imaginary(), 1e-5F);
  EXPECT_EQ(transform.local_rot().real(), rot.real());
  EXPECT_VEC_NEAR(transform.local_scale(), scale, 1e-5F);
}

TEST_F(TransformTest, DefaultMatricesAreIdentity) {
  // given / when
  const Transform3f transform;

  // then
  EXPECT_MAT_NEAR(transform.local_to_parent_matrix(), Mat4f::identity(), 1e-5F);
  EXPECT_MAT_NEAR(transform.local_to_world_matrix(), Mat4f::identity(), 1e-5F);
  EXPECT_MAT_NEAR(transform.parent_to_local_matrix(), Mat4f::identity(), 1e-5F);
  EXPECT_MAT_NEAR(transform.world_to_local_matrix(), Mat4f::identity(), 1e-5F);
}

/**
 * Transformations
 */

TEST_F(TransformTest, MatricesAreCalculatedProperly) {
  // given
  // calculated on paper
  const Mat4f parent_expected(Vec4f(0, 0, -2, 0), Vec4f(0, 2, 0, 0), Vec4f(2, 0, 0, 0), Vec4f(1, 2, 3, 1));
  const Mat4f expected(Vec4f(0, 0, -2, 0), Vec4f(2, 0, 0, 0), Vec4f(0, -2, 0, 0), Vec4f(1, 2, 3, 1));

  // when
  transform_.set_local_rot(Quatf::from_euler_xyz(Vec3f(kPi / 2, 0, 0)));

  // then
  EXPECT_MAT_NEAR(parent_expected, parent_transform_.local_to_parent_matrix(), 1e-5F);
  EXPECT_MAT_NEAR(expected, transform_.local_to_world_matrix(), 1e-5F);
}

TEST_F(TransformTest, InversesAreCalculatedProperly) {
  // given
  auto identity = Mat4f::identity();

  // when / then
  EXPECT_MAT_NEAR(identity, parent_transform_.local_to_parent_matrix() * parent_transform_.parent_to_local_matrix(),
                  1e-5F);
  EXPECT_MAT_NEAR(identity, parent_transform_.local_to_world_matrix() * parent_transform_.world_to_local_matrix(),
                  1e-5F);
  EXPECT_MAT_NEAR(identity, transform_.local_to_parent_matrix() * transform_.parent_to_local_matrix(), 1e-5F);
  EXPECT_MAT_NEAR(identity, transform_.local_to_world_matrix() * transform_.world_to_local_matrix(), 1e-5F);
}

TEST_F(TransformTest, LocalOrientationIsCalculatedProperly) {
  // given
  transform_.set_local_rot(Quatf::from_euler_xyz(Vec3f(kPi / 2, 0, 0)));
  Vec3f front = transform_.local_front();
  Vec3f right = transform_.local_right();
  Vec3f up    = transform_.local_up();

  // when
  Mat3f orientation = transform_.local_orientation();

  // then
  EXPECT_VEC_NEAR(right, orientation[0], 1e-5F);
  EXPECT_VEC_NEAR(up, orientation[1], 1e-5F);
  EXPECT_VEC_NEAR(front, orientation[2], 1e-5F);
}

TEST_F(TransformTest, OrientationIsCalculatedProperly) {
  // given
  transform_.set_local_rot(Quatf::from_euler_xyz(Vec3f(kPi / 2, 0, 0)));
  Vec3f front = transform_.front();
  Vec3f right = transform_.right();
  Vec3f up    = transform_.up();

  // when
  Mat3f orientation = transform_.orientation();

  // then
  EXPECT_VEC_NEAR(right, orientation[0], 1e-5F);
  EXPECT_VEC_NEAR(up, orientation[1], 1e-5F);
  EXPECT_VEC_NEAR(front, orientation[2], 1e-5F);
}

TEST_F(TransformTest, PosIsTransformedProperly) {
  // given
  const Vec3f position(1, 1, 1);
  // initial:    (1, 1, 1)
  // scaled:     (2, 2, 2)
  // rotated:    (2, 2,-2)
  // translated: (3, 4, 1)
  Vec3f expected(3, 4, 1);

  // when
  transform_.set_local_pos(position);

  // then
  EXPECT_VEC_NEAR(expected, transform_.pos(), 1e-5F);
}

TEST_F(TransformTest, RotIsTransformedProperly) {
  // given
  // rotation of 90deg around X axis
  const auto rot = Quatf::rotation_x(kPi / 2);
  // rotation of 120deg around axis of (1,1,-1)
  auto axis     = normalize(Vec3f(1, 1, -1));
  auto expected = Quatf::rotation_axis(2 * kPi / 3, axis);

  // when
  transform_.set_local_rot(rot);

  // then
  EXPECT_ROT_NEAR(expected, transform_.rot(), 1e-5F);
}

TEST_F(TransformTest, ScaleIsTransformedProperly) {
  // given
  const auto scale    = Vec3f::filled(3.F);
  const auto expected = Vec3f::filled(6.F);

  // when
  transform_.set_local_scale(scale);

  // then
  EXPECT_VEC_NEAR(expected, transform_.scale(), 1e-5F);
}
