#include <gtest/gtest.h>

#include <liberay/vkren/scene/transform_tree.hpp>

using FlatTree      = eray::vkren::FlatTree;
using TransformTree = eray::vkren::TransformTree;
using Transform     = eray::vkren::Transform;
using NodeId        = eray::vkren::NodeId;
namespace math      = eray::math;

// Matrix comparison macro: element-wise check with tolerance
#define EXPECT_MAT4_NEAR(expected, actual, eps)                                                     \
  do {                                                                                              \
    for (int i = 0; i < 16; ++i) {                                                                  \
      auto e = (expected).nth(i);                                                                   \
      auto a = (actual).nth(i);                                                                     \
      EXPECT_NEAR(e, a, eps) << "Mismatch at index " << i << ": expected=" << e << " actual=" << a; \
    }                                                                                               \
  } while (0)

TEST(TransformTreeTest, LocalModelMatrixUpdatedWhenSetLocalTransform) {
  auto tree = TransformTree::create(8);
  NodeId n  = tree.create_node();

  Transform t{};
  t.position = math::Vec3f(1.F, 2.F, 3.F);
  t.rotation = math::Quatf();  // identity quaternion
  t.scale    = math::Vec3f(2.F, 2.F, 2.F);

  tree.set_local_transform(n, t);
  tree.update();

  const math::Mat4f expected_local_model =
      math::translation(t.position) * math::rot_mat_from_quat(t.rotation) * math::scale(t.scale);

  EXPECT_MAT4_NEAR(expected_local_model, tree.local_to_parent_matrix(n), 1e-5);
}

TEST(TransformTreeTest, WorldModelMatrixPropagatesThroughHierarchy) {
  auto tree = TransformTree::create(16);

  NodeId parent = tree.create_node();
  NodeId child  = tree.create_node(parent);

  Transform ptrans{};
  ptrans.position = math::Vec3f(1.F, 0.F, 0.F);
  ptrans.rotation = math::Quatf();
  ptrans.scale    = math::Vec3f(1.F, 1.F, 1.F);
  tree.set_local_transform(parent, ptrans);

  Transform ctrans{};
  ctrans.position = math::Vec3f(0.F, 1.F, 0.F);
  ctrans.rotation = math::Quatf();
  ctrans.scale    = math::Vec3f(1.F, 1.F, 1.F);
  tree.set_local_transform(child, ctrans);

  tree.update();

  const math::Mat4f parent_local =
      math::translation(ptrans.position) * math::rot_mat_from_quat(ptrans.rotation) * math::scale(ptrans.scale);

  EXPECT_MAT4_NEAR(parent_local, tree.local_to_world_matrix(parent), 1e-5);

  const math::Mat4f child_local =
      math::translation(ctrans.position) * math::rot_mat_from_quat(ctrans.rotation) * math::scale(ctrans.scale);

  const math::Mat4f expected_child_world = parent_local * child_local;
  EXPECT_MAT4_NEAR(expected_child_world, tree.local_to_world_matrix(child), 1e-5);

  const math::Mat4f parent_local_inv = math::scale(1.F / (ptrans.scale + 0.000001F)) *
                                       math::rot_mat_from_quat(math::conjugate(ptrans.rotation)) *
                                       math::translation(-ptrans.position);

  const math::Mat4f child_local_inv = math::scale(1.F / (ctrans.scale + 0.000001F)) *
                                      math::rot_mat_from_quat(math::conjugate(ctrans.rotation)) *
                                      math::translation(-ctrans.position);

  const math::Mat4f expected_child_world_inv = child_local_inv * parent_local_inv;
  EXPECT_MAT4_NEAR(expected_child_world_inv, tree.world_to_local_matrix(child), 1e-5);
}
