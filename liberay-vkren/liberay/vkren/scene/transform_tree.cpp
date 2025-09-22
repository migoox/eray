#include <algorithm>
#include <liberay/math/mat.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <liberay/vkren/scene/transform_tree.hpp>
#include <utility>

namespace eray::vkren {

NodeId TransformTree::create_node(NodeId parent_id) {
  auto node_id = tree_.create_node(parent_id);
  auto index   = FlatTree::node_index_of(node_id);

  local_transforms_[index].position = math::Vec3f(0.F, 0.F, 0.F);
  local_transforms_[index].rotation = math::Quatf();
  local_transforms_[index].scale    = math::Vec3f(1.F, 1.F, 1.F);

  dirty_nodes_.insert(node_id);

  return node_id;
}

uint32_t TransformTree::node_level(NodeId node_id) const { return tree_.node_level(node_id); }

void TransformTree::copy_node(NodeId node_id, NodeId parent_id) {
  auto new_node_id = tree_.copy_node(node_id, parent_id);
  dirty_nodes_.insert(new_node_id);
}

void TransformTree::delete_node(NodeId node_id) { tree_.delete_node(node_id); }

void TransformTree::change_parent(NodeId node_id, NodeId parent_id) {
  tree_.change_parent(node_id, parent_id);
  dirty_nodes_.insert(node_id);
}

void TransformTree::make_orphan(NodeId node_id) {
  tree_.make_orphan(node_id);
  dirty_nodes_.insert(node_id);
}

const std::vector<NodeId>& TransformTree::nodes_bfs_order() const { return tree_.nodes_bfs_order(); }

const std::vector<NodeId>& TransformTree::nodes_dfs_preorder() const { return tree_.nodes_dfs_preorder(); }

const Transform& TransformTree::local_transform(NodeId node_id) {
  return local_transforms_[FlatTree::node_index_of(node_id)];
}

const Transform& TransformTree::world_transform(NodeId node_id) {
  return world_transforms_[FlatTree::node_index_of(node_id)];
}

void TransformTree::set_local_transform(NodeId node_id, Transform transform) {
  local_transforms_[FlatTree::node_index_of(node_id)] = std::move(transform);
  dirty_nodes_.insert(node_id);
}

void TransformTree::set_name(NodeId node_id, std::string name) {
  name_[FlatTree::node_index_of(node_id)] = std::move(name);
}

void TransformTree::update() {
  // Update local model matrices
  for (auto node : dirty_nodes_) {
    auto index        = FlatTree::node_index_of(node);
    const auto& trans = local_transforms_[index];
    local_model_mats_[index] =
        math::translation(trans.position) * math::rot_mat_from_quat(trans.rotation) * math::scale(trans.scale);
    local_model_inv_mats_[index] = math::scale(1.F / (trans.scale + 0.000001F)) *
                                   math::rot_mat_from_quat(math::conjugate(trans.rotation)) *
                                   math::translation(-trans.position);
  }

  // Sort dirty nodes by their level
  dirty_nodes_helper_.clear();
  if (dirty_nodes_helper_.capacity() < dirty_nodes_.size()) {
    dirty_nodes_helper_.reserve(dirty_nodes_.size());
  }
  std::ranges::copy(dirty_nodes_, std::back_inserter(dirty_nodes_helper_));
  std::ranges::sort(dirty_nodes_helper_,
                    [this](auto n1, auto n2) { return tree_.node_level(n1) > tree_.node_level(n2); });

  // Update world model matrices
  for (auto node : dirty_nodes_helper_) {
    if (!dirty_nodes_.contains(node)) {
      continue;
    }

    for (auto descendant : FlatTreeBFSRange{&tree_, node}) {
      if (auto descendant_it = dirty_nodes_.find(descendant); descendant_it != dirty_nodes_.end()) {
        dirty_nodes_.erase(descendant_it);
      }
      auto descendant_index = FlatTree::node_index_of(descendant);
      auto parent_index     = FlatTree::node_index_of(tree_.parent_of(descendant));

      world_model_mats_[descendant_index] = world_model_mats_[parent_index] * local_model_mats_[descendant_index];
      world_model_inv_mats_[descendant_index] =
          local_model_inv_mats_[descendant_index] * world_model_inv_mats_[parent_index];
    }
  }

  dirty_nodes_.clear();
}

void TransformTree::set_local_position(NodeId node_id, math::Vec3f position) {
  local_transforms_[FlatTree::node_index_of(node_id)].position = std::move(position);
}

void TransformTree::set_local_rotation(NodeId node_id, math::Quatf rotation) {
  local_transforms_[FlatTree::node_index_of(node_id)].rotation = std::move(rotation);
}

void TransformTree::set_local_scale(NodeId node_id, math::Vec3f scale) {
  local_transforms_[FlatTree::node_index_of(node_id)].scale = std::move(scale);
}

const math::Mat4f& TransformTree::local_to_parent_matrix(NodeId node_id) {
  return local_model_mats_[FlatTree::node_index_of(node_id)];
}

const math::Mat4f& TransformTree::parent_to_local_matrix(NodeId node_id) {
  return local_model_inv_mats_[FlatTree::node_index_of(node_id)];
}

const math::Mat4f& TransformTree::local_to_world_matrix(NodeId node_id) {
  return world_model_mats_[FlatTree::node_index_of(node_id)];
}

const math::Mat4f& TransformTree::world_to_local_matrix(NodeId node_id) {
  return world_model_inv_mats_[FlatTree::node_index_of(node_id)];
}

}  // namespace eray::vkren
