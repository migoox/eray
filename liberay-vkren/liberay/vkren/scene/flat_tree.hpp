#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace eray::vkren {

/**
 * @brief This tree ALWAYS has a root node (`kRootNodeIndex`). Manages tree nodes hierarchy only. No values are stored
 * here.
 *
 */
class FlatTree {
 public:
  using NodeIndex = uint32_t;

  /**
   * @brief LSB 32 bits store node index and the rest of the 32 bits store the node version.
   *
   */
  using NodeId = uint64_t;

  static constexpr NodeIndex kRootNodeIndex = 0;
  static constexpr NodeId kRootNodeId       = kRootNodeIndex;
  static constexpr NodeIndex kNullNodeIndex = std::numeric_limits<NodeIndex>::max();
  static constexpr NodeId kNullNodeId       = kNullNodeIndex;

  // https://opendsa-server.cs.vt.edu/ODSA/Books/Everything/html/GenTreeImplement.html
  struct Node {
    NodeIndex parent{kNullNodeIndex};
    NodeIndex right_child{kNullNodeIndex};
    NodeIndex left_sibling{kNullNodeIndex};
    NodeIndex right_sibling{kNullNodeIndex};
  };

  explicit FlatTree(std::nullptr_t) {}

  /**
   * @brief Returns pair, where first element denotes index and the second one denotes the node version.
   *
   * @param node
   * @return std::pair<uint32_t, uint32_t>
   */
  [[nodiscard]] static std::pair<uint32_t, uint32_t> decompose_node_id(NodeId node) {
    auto index   = static_cast<uint32_t>(node & 0xFFFFFFFF);
    auto version = static_cast<uint32_t>(node >> 32);
    return {index, version};
  }

  [[nodiscard]] static uint32_t extract_node_index(NodeId node) {
    auto index = static_cast<uint32_t>(node & 0xFFFFFFFF);
    return index;
  }

  [[nodiscard]] static uint32_t extract_node_version(NodeId node) {
    auto version = static_cast<uint32_t>(node >> 32);
    return version;
  }

  [[nodiscard]] static uint64_t compose_node_id(NodeIndex index, uint32_t version) {
    return (static_cast<uint64_t>(version) << 32) | static_cast<uint64_t>(index);
  }

  [[nodiscard]] static FlatTree create(size_t max_nodes_count);

  [[nodiscard]] NodeId create_node(NodeId parent_id);
  [[nodiscard]] NodeId create_node() { return create_node(kRootNodeIndex); }

  const Node& node_info(NodeId node_id) const;
  [[nodiscard]] uint32_t node_level(NodeId node_id) const;

  /**
   * @brief Deletes node.
   *
   * @warning Root node must not be deleted.
   *
   * @param node_id
   */
  void delete_node(NodeId node_id);

  /**
   * @brief Root becomes a parent of the node with index `node_id`.
   *
   * @param index
   * @return NodeIndex
   */
  void make_orphan(NodeId node_id);

  /**
   * @brief Changes the parent of the node with index `node_id` to the node with index `parent_id`.
   *
   * @param node_id
   * @param parent_id
   * @return NodeIndex
   */
  void change_parent(NodeId node_id, NodeId parent_id);

  const std::vector<NodeId>& nodes_bfs_order() const;
  const std::vector<NodeId>& nodes_dfs_preorder() const;

  bool exists(NodeId node_id) const;

 private:
  uint64_t index2id(NodeIndex index) const { return compose_node_id(index, version_[index]); }

  FlatTree();

  std::vector<Node> nodes_;
  std::vector<uint32_t> level_;
  std::vector<NodeIndex> version_;
  size_t node_count_{};

  std::vector<NodeIndex> free_nodes_;

  mutable std::vector<NodeId> dfs_preorder_cached_;
  mutable std::vector<NodeId> bfs_order_cached_;
  mutable bool is_dirty_{};
};

}  // namespace eray::vkren
