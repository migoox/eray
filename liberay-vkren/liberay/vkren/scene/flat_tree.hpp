#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <stack>
#include <vector>

namespace eray::vkren {

using NodeIndex = uint32_t;

/**
 * @brief LSB 32 bits store node index and the rest of the 32 bits store the node version.
 *
 */
using NodeId = uint64_t;

/**
 * @brief This tree ALWAYS has a root node (`kRootNodeId`) which is a parent for every orphaned node. This manages tree
 * nodes hierarchy only and no values are stored here.
 *
 */
class FlatTree {
 public:
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

  [[nodiscard]] static uint32_t node_index_of(NodeId node) {
    auto index = static_cast<uint32_t>(node & 0xFFFFFFFF);
    return index;
  }

  [[nodiscard]] static uint32_t node_version_of(NodeId node) {
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
  NodeId parent_of(NodeId node_id) const;
  NodeId left_sibling_of(NodeId node_id) const;
  NodeId right_sibling_of(NodeId node_id) const;
  [[nodiscard]] uint32_t node_level(NodeId node_id) const;

  /**
   * @brief Recursively copies the node with `node_id` and makes a node with `parent_id` its parent.
   *
   * @param node_id
   * @param parent_id
   */
  [[nodiscard]] NodeId copy_node(NodeId node_id, NodeId parent_id);

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
   * @brief Changes the parent of the node with index `node_id` to the node with index `parent_id`.
   *
   * @param node_id
   * @param parent_id
   * @return NodeIndex
   */
  void change_parent(NodeId node_id, NodeId parent_id);

  const std::vector<NodeId>& nodes_bfs_order() const;
  const std::vector<NodeId>& nodes_dfs_preorder() const;

  [[nodiscard]] bool exists(NodeId node_id) const;

  [[nodiscard]] uint64_t index_to_id(NodeIndex index) const { return compose_node_id(index, version_[index]); }

 private:
  void set_dirty();

  FlatTree();

  std::vector<Node> nodes_;
  std::vector<uint32_t> level_;
  std::vector<NodeIndex> version_;
  size_t node_count_{};

  std::vector<NodeIndex> free_nodes_;

  mutable std::vector<NodeId> dfs_preorder_cached_;
  mutable std::vector<NodeId> bfs_order_cached_;
  mutable bool dfs_dirty_{};
  mutable bool bfs_dirty_{};
};

/**
 * @brief Allows to iterate over the nodes in the with DFS preorder scheme.
 *
 */
class FlatTreeDFSIterator {
 public:
  using value_type        = NodeId;
  using reference         = const NodeId&;
  using pointer           = const NodeId*;
  using iterator_category = std::forward_iterator_tag;
  using difference_type   = std::ptrdiff_t;

  FlatTreeDFSIterator() = default;
  FlatTreeDFSIterator(const FlatTree* tree, NodeId start);

  reference operator*() const { return current_; }
  pointer operator->() const { return &current_; }

  FlatTreeDFSIterator& operator++() {
    advance();
    return *this;
  }
  FlatTreeDFSIterator operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
  }
  bool operator==(const FlatTreeDFSIterator& other) const { return current_ == other.current_ && tree_ == other.tree_; }
  bool operator!=(const FlatTreeDFSIterator& other) const { return !(*this == other); }

 private:
  void advance();
  const FlatTree* tree_{nullptr};
  std::stack<NodeId> stack_;
  NodeId current_{FlatTree::kNullNodeId};
};

/**
 * @brief Allows to iterate over the nodes in the with DFS preorder scheme.
 *
 */
struct FlatTreeDFSRange {
  const FlatTree* tree_{nullptr};
  NodeId root_ = FlatTree::kNullNodeId;

  FlatTreeDFSRange(const FlatTree* tree, NodeId root) : tree_(tree), root_(root) {}

  FlatTreeDFSIterator begin() const { return FlatTreeDFSIterator(tree_, root_); }
  FlatTreeDFSIterator end() const { return FlatTreeDFSIterator(); }
};

/**
 * @brief Allows to iterate over the nodes in the with BFS scheme.
 *
 */
class FlatTreeBFSIterator {
 public:
  using value_type        = NodeId;
  using reference         = const NodeId&;
  using pointer           = const NodeId*;
  using iterator_category = std::forward_iterator_tag;
  using difference_type   = std::ptrdiff_t;

  FlatTreeBFSIterator() = default;
  FlatTreeBFSIterator(const FlatTree* tree, NodeId start);

  reference operator*() const { return current_; }
  pointer operator->() const { return &current_; }

  FlatTreeBFSIterator& operator++() {
    advance();
    return *this;
  }
  FlatTreeBFSIterator operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
  }
  bool operator==(const FlatTreeBFSIterator& other) const { return current_ == other.current_ && tree_ == other.tree_; }
  bool operator!=(const FlatTreeBFSIterator& other) const { return !(*this == other); }

 private:
  void advance();
  const FlatTree* tree_{nullptr};
  std::queue<NodeId> queue_;
  NodeId current_{FlatTree::kNullNodeId};
};

/**
 * @brief Allows to iterate over the nodes in the with BFS scheme.
 *
 */
struct FlatTreeBFSRange {
  const FlatTree* tree_{nullptr};
  NodeId root_ = FlatTree::kNullNodeId;

  FlatTreeBFSRange(const FlatTree* tree, NodeId root) : tree_(tree), root_(root) {}

  FlatTreeBFSIterator begin() const { return FlatTreeBFSIterator(tree_, root_); }
  FlatTreeBFSIterator end() const { return FlatTreeBFSIterator(); }
};

}  // namespace eray::vkren
