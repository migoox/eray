#pragma once

#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/scene/camera.hpp>
#include <liberay/vkren/scene/entity_pool.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <liberay/vkren/scene/light.hpp>
#include <liberay/vkren/scene/material.hpp>
#include <liberay/vkren/scene/sparse_set.hpp>
#include <liberay/vkren/scene/transform_tree.hpp>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

template <typename... TValues>
using EntitySparseSet = SparseSet<EntityIndex, TValues...>;

struct Scene {
 public:
  const TransformTree& tree() const { return tree_; }
  TransformTree& tree() { return tree_; }

 private:
  TransformTree tree_;
  EntitySparseSet<Camera> camera_nodes_;
  EntitySparseSet<Light> light_nodes_;
};

}  // namespace eray::vkren
