#pragma once

#include <cstddef>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <unordered_set>

namespace eray::vkren {

struct Transform {
  math::Vec3f position;
  math::Quatf rotation;
  math::Vec3f scale;
};

class TransformTree {
 public:
  explicit TransformTree(std::nullptr_t) {}
  [[nodiscard]] static TransformTree create(size_t max_nodes_count);

  [[nodiscard]] NodeId create_node(NodeId parent_id);
  [[nodiscard]] NodeId create_node() { return create_node(FlatTree::kRootNodeId); }
  [[nodiscard]] uint32_t node_level(NodeId node_id) const;

  /**
   * @brief Recursively copies the node with `node_id` and makes a node with `parent_id` its parent.
   *
   * @param node_id
   * @param parent_id
   */
  void copy_node(NodeId node_id, NodeId parent_id);

  /**
   * @brief Deletes node with all descendants.
   *
   * @warning Root node must not be deleted. When called with root id this function produces undefined behaviour.
   *
   * @param node_id
   */
  void delete_node(NodeId node_id);

  /**
   * @brief Root becomes a parent of the node with `node_id`.
   *
   * @param index
   * @return NodeIndex
   */
  void make_orphan(NodeId node_id);

  /**
   * @brief Root becomes a parent of the node with `node_id`. The global transform of the node with `node_id` will not
   * change.
   *
   * @param index
   * @return NodeIndex
   */
  void change_parent(NodeId node_id, NodeId parent_id);

  const std::vector<NodeId>& nodes_bfs_order() const;

  const std::vector<NodeId>& nodes_dfs_preorder() const;

  /**
   * @brief Checks whether a node with `node_id` still exists.
   *
   * @param node_id
   * @return true
   * @return false
   */
  bool exists(NodeId node_id) const;

  /**
   * @brief Returns local transform of the node. This function does not call `update()` implicitly.
   *
   * @return Transform
   */
  const Transform& local_transform(NodeId node_id);

  /**
   * @brief Returns world transform of the node. This function does not call `update()` implicitly.
   *
   * @return Transform
   */
  const Transform& world_transform(NodeId node_id);

  /**
   * @brief Returns model matrix of the node.
   *
   * @return math::Mat4f
   */
  const math::Mat4f& local_to_parent_matrix(NodeId node_id);

  /**
   * @brief Returns inverse of the model matrix of the node.
   *
   * @return math::Mat4f
   */
  const math::Mat4f& parent_to_local_matrix(NodeId node_id);

  /**
   * @brief Returns global matrix of the node. This function does not call `update()` implicitly.
   *
   * @return math::Mat4f
   */
  const math::Mat4f& local_to_world_matrix(NodeId node_id);

  /**
   * @brief Returns inverse of the `local_to_world_matrix`.
   *
   * @return math::Mat4f
   */
  const math::Mat4f& world_to_local_matrix(NodeId node_id);

  /**
   * @brief Returns all current global transformation matrices. This function does not call `update()` implicitly.
   * Useful when passing buffer of transforms to the GPU memory.
   *
   * @return const std::vector<math::Mat4f>&
   */
  const std::vector<math::Mat4f>& local_to_world_matrices();
  const std::vector<math::Mat4f>& world_to_local_matrices();

  void set_local_transform(NodeId node_id, Transform transform);
  void set_local_position(NodeId node_id, math::Vec3f position);
  void set_local_rotation(NodeId node_id, math::Quatf rotation);
  void set_local_scale(NodeId node_id, math::Vec3f scale);

  /**
   * @brief Sets node's name
   *
   * @param id
   * @param name
   */
  void set_name(NodeId node_id, std::string name);

  /**
   * @brief Updates dirty transforms.
   *
   */
  void update();

 private:
  TransformTree() = default;

  FlatTree tree_ = FlatTree(nullptr);
  std::vector<Transform> local_transforms_;
  std::vector<Transform> world_transforms_;

  std::vector<math::Mat4f> local_model_mats_;
  std::vector<math::Mat4f> world_model_mats_;
  std::vector<math::Mat4f> local_model_inv_mats_;
  std::vector<math::Mat4f> world_model_inv_mats_;

  std::vector<std::string> name_;

  std::unordered_set<NodeId> dirty_nodes_;
  std::vector<NodeId> dirty_nodes_helper_;

  size_t nodes_created_count_{};
};

}  // namespace eray::vkren
