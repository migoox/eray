#pragma once

#include <liberay/math/quat.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>

namespace eray::vkren {

struct Transform {
  math::Quatf rotation;
  math::Vec3f position;
  math::Vec3f scale;
};

class TransformTree : public FlatTree {
 public:
  [[nodiscard]] NodeId create_node(NodeId parent_id);
  [[nodiscard]] NodeId create_node() { return create_node(FlatTree::kRootNodeId); }
  [[nodiscard]] uint32_t node_level(NodeId node_id) const;
  void copy_node(NodeId node_id, NodeId parent_id);
  void delete_node(NodeId node_id);
  void make_orphan(NodeId node_id);
  void change_parent(NodeId node_id, NodeId parent_id);
  const std::vector<NodeId>& nodes_bfs_order() const;
  const std::vector<NodeId>& nodes_dfs_preorder() const;
  bool exists(NodeId node_id) const;

  /**
   * @brief Returns local transform of the node. This function does not call `update()` implicitly.
   *
   * @return Transform
   */
  Transform loc_transform(NodeId);

  /**
   * @brief Returns global transform of the node. This function does not call `update()` implicitly.
   *
   * @return Transform
   */
  Transform glob_transform(NodeId);

  /**
   * @brief Returns global matrix of the node. This function does not call `update()` implicitly.
   *
   * @return math::Mat4f
   */
  math::Mat4f glob_mat(NodeId);

  /**
   * @brief Returns all current global transformation matrices. This function does not call `update()` implicitly.
   * Useful when passing buffer of transforms to the GPU memory.
   *
   * @return const std::vector<math::Mat4f>&
   */
  const std::vector<math::Mat4f>& transform_mats();
  const std::vector<math::Mat4f>& transform_inv_mats();

  /**
   * @brief Updates the local transform. This function marks the tree as dirty.
   *
   * @param id
   * @param transform
   */
  void set_loc_transform(NodeId id, Transform transform);

  /**
   * @brief Sets node's name
   *
   * @param id
   * @param name
   */
  void set_name(NodeId id, std::string name);

  /**
   * @brief Updates dirty transforms.
   *
   */
  void update();

 private:
  std::vector<Transform> loc_transforms_;
  std::vector<Transform> glob_transforms_;
  std::vector<Transform> glob_transform_mats_;
  std::vector<Transform> glob_transform_inv_mats_;
};

}  // namespace eray::vkren
