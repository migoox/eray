#pragma once

#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/util/zstring_view.hpp>
#include <limits>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

using NodeId     = std::uint32_t;
using MeshId     = std::uint32_t;
using MaterialId = std::uint32_t;

auto constexpr kNullEntityId = std::numeric_limits<std::uint32_t>::max();

// https://opendsa-server.cs.vt.edu/ODSA/Books/Everything/html/GenTreeImplement.html
struct Node {
  NodeId left_child_id;
  NodeId right_sibling_id;
  NodeId parent_id;
};

struct NodeInfo {
  NodeId id;
  util::zstring_view name;
  int level;
};

struct Transform {
  math::Quatf rotation;
  math::Vec3f position;
  math::Vec3f scale;
};

struct SceneTreeSystem {
  // NOTE: Designed only for dynamic nodes scene hierarchy, do not use for animation (e.g. bone is a node approach).
  // Animations needs their own system, such a system has completely different requirements, e.g. there is no need for
  // changing the transform parent, also the client does not care about the CRUD operations of bones.
  //
  // TL;DR: Never treat skeleton animation bones as nodes.

  void create_node(NodeId parent = kNullEntityId);

  NodeInfo info(NodeId);

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

  /**
   * @brief Returns node infos in preorder tree traversal order.
   *
   * @return const std::vector<NodeInfo>&
   */
  const std::vector<NodeInfo>& node_infos_preorder();

  void set_loc_transform(NodeId id, Transform transform);
  void set_name(NodeId id, std::string name);

  /**
   * @brief If `new_parent` is provided, changes the parent, makes a root otherwise. The orientation and the position
   * (global matrix) of the node always stays the same.
   *
   * @param id
   * @param new_parent
   */
  void reattach(NodeId id, NodeId new_parent = kNullEntityId);

  /**
   * @brief Deletes the node with provided `id`.
   *
   * @param id
   */
  void remove(NodeId id);

  /**
   * @brief Updates dirty transforms.
   *
   */
  void update();

  struct SceneTree {
    // nodes[node_id: NodeId] stores the node
    std::vector<Node> nodes;
    std::vector<Transform> local_transforms;
    std::vector<Transform> global_transforms;
    std::vector<Transform> global_transform_mats;
    std::vector<NodeInfo> node_info;
    std::vector<std::string> names;

    std::vector<NodeId> free_nodes;
  } tree;  // for tree hierarchy maintainence

  std::vector<std::pair<NodeId, Transform>> dirty_transforms;  // NOTE: Order doesn't matter

  std::vector<NodeInfo> nodes_preorder_cache;
};

struct Mesh {
  NodeId mesh_primitive_id;

  // in the future: array of weights to be applied to the morph targets
};

struct MeshPrimitive {
  vk::Buffer index_buffer;
  uint32_t index_count;
  uint32_t first_index;
};

struct Material {
  vk::Pipeline pipeline;
  vk::PipelineLayout layout;
  vk::DescriptorSet material_set;
};

}  // namespace eray::vkren
