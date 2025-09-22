#include <liberay/vkren/scene/transform_tree.hpp>

namespace eray::vkren {

void TransformTree::copy_node(NodeId node_id, NodeId parent_id) {}

void TransformTree::delete_node(NodeId node_id) {}

void TransformTree::make_orphan(NodeId node_id) {}

void TransformTree::change_parent(NodeId node_id, NodeId parent_id) {}

const std::vector<NodeId>& TransformTree::nodes_bfs_order() const {}

const std::vector<NodeId>& TransformTree::nodes_dfs_preorder() const {}

Transform TransformTree::loc_transform(NodeId) {}

Transform TransformTree::glob_transform(NodeId) {}

math::Mat4f TransformTree::glob_mat(NodeId) {}

const std::vector<math::Mat4f>& TransformTree::transform_mats() {}

void TransformTree::set_loc_transform(NodeId id, Transform transform) {}

void TransformTree::set_name(NodeId id, std::string name) {}

void TransformTree::update() {}

}  // namespace eray::vkren
