#include <gtest/gtest.h>

#include <liberay/math/transform3.hpp>
#include <liberay/math/vec.hpp>
#include <tests/helpers/math_helpers.hpp>

using namespace eray::math;  // NOLINT

class SizeTest : public testing::Test {};

/**
 * Defaults
 */

TEST_F(SizeTest, VectorsHaveProperSize) {
  // given / when / then
  EXPECT_EQ(sizeof(Vec2f), 8);
  EXPECT_EQ(sizeof(Vec3f), 12);
  EXPECT_EQ(sizeof(Vec4f), 16);

  EXPECT_EQ(sizeof(Vec2i), 8);
  EXPECT_EQ(sizeof(Vec3i), 12);
  EXPECT_EQ(sizeof(Vec4i), 16);

  EXPECT_EQ(sizeof(Vec2u), 8);
  EXPECT_EQ(sizeof(Vec3u), 12);
  EXPECT_EQ(sizeof(Vec4u), 16);

  EXPECT_EQ(sizeof(Vec2d), 16);
  EXPECT_EQ(sizeof(Vec3d), 24);
  EXPECT_EQ(sizeof(Vec4d), 32);
}
