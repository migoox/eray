#pragma once

#include <cstddef>
#include <cstdint>
#include <liberay/vkren/scene/entity_pool.hpp>
#include <queue>
#include <stack>
#include <vector>

namespace eray::vkren {

class FlatTreeDFSIterator;
class FlatTreeBFSIterator;

struct NodeSurroundingInfo {
  NodeId parent_id;
  NodeId left_child_id;
  NodeId right_child_id;
  NodeId left_sibling_id;
  NodeId right_sibling_id;
};

/**
 * @brief This tree ALWAYS has a root node (`kRootNodeId`) which is a parent for every orphaned node. This manages tree
 * nodes hierarchy only and no values are stored here.
 *
 */
class FlatTree {
 public:
  static constexpr size_t kNullNodeIndex = EntityPool<NodeId>::kNullIndex;
  static constexpr NodeId kNullNodeId    = EntityPool<NodeId>::kNullId;
  static constexpr size_t kRootNodeIndex = 0;
  static constexpr NodeId kRootNodeId    = NodeId{0};

  // https://opendsa-server.cs.vt.edu/ODSA/Books/Everything/html/GenTreeImplement.html
  struct Node {
    size_t parent{kNullNodeIndex};
    size_t left_child{kNullNodeIndex};
    size_t right_child{kNullNodeIndex};
    size_t left_sibling{kNullNodeIndex};
    size_t right_sibling{kNullNodeIndex};
  };

  explicit FlatTree(std::nullptr_t) {}

  [[nodiscard]] static FlatTree create(size_t max_nodes_count);

  [[nodiscard]] NodeId create_node(NodeId parent_id);
  [[nodiscard]] NodeId create_node() { return create_node(kRootNodeId); }

  NodeSurroundingInfo node_surrounding_info(NodeId node_id) const;
  NodeId parent_of(NodeId node_id) const;
  std::optional<NodeId> left_sibling_of(NodeId node_id) const;
  std::optional<NodeId> right_sibling_of(NodeId node_id) const;
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
   * @warning If `parent_id` is a
   * descendant of `node_id` this function produces an undefined behaviour.
   *
   * @param node_id
   * @param parent_id
   * @return NodeIndex
   */
  void change_parent(NodeId node_id, NodeId parent_id);

  const std::vector<NodeId>& nodes_bfs_order() const;
  const std::vector<NodeId>& nodes_dfs_preorder() const;

  /**
   * @brief If `node_id` equals `ancestor_id` then it's not an descendant. This function iterates over the subtree so
   * it's heavy.
   *
   * @param node_id
   * @param ancestor_id
   * @return true
   * @return false
   */
  [[nodiscard]] bool is_descendant(NodeId node_id, NodeId ancestor_id) const;

  [[nodiscard]] bool exists(NodeId node_id) const;

  [[nodiscard]] std::optional<NodeId> compose_id(size_t index) const {
    auto result = nodes_pool_.compose_id(index);
    if (result == kNullNodeId) {
      return std::nullopt;
    }
    return result;
  }

  [[nodiscard]] static size_t index_of(NodeId id) { return EntityPool<NodeId>::index_of(id); }
  [[nodiscard]] static uint32_t version_of(NodeId id) { return EntityPool<NodeId>::version_of(id); }

 private:
  FlatTree() = default;
  void set_dirty();

  friend FlatTreeDFSIterator;
  friend FlatTreeBFSIterator;

  EntityPool<NodeId> nodes_pool_ = EntityPool<NodeId>(nullptr);

  std::vector<Node> nodes_;
  std::vector<uint32_t> level_;

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
  FlatTreeDFSIterator(const FlatTree* tree, NodeId start, bool inclusive = true);

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
  bool operator==(const FlatTreeDFSIterator& other) const { return current_ == other.current_; }
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
  NodeId root_    = FlatTree::kNullNodeId;
  bool inclusive_ = true;

  FlatTreeDFSRange(const FlatTree* tree, NodeId root, bool inclusive = true)
      : tree_(tree), root_(root), inclusive_(inclusive) {}

  FlatTreeDFSIterator begin() const { return FlatTreeDFSIterator(tree_, root_, inclusive_); }
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
  FlatTreeBFSIterator(const FlatTree* tree, NodeId start, bool inclusive = true, bool dir_left_to_right = true);

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
  bool operator==(const FlatTreeBFSIterator& other) const { return current_ == other.current_; }
  bool operator!=(const FlatTreeBFSIterator& other) const { return !(*this == other); }

 private:
  void advance();
  const FlatTree* tree_{nullptr};
  std::queue<NodeId> queue_;
  NodeId current_{FlatTree::kNullNodeId};
  bool dir_left_to_right_{true};
};

/**
 * @brief Allows to iterate over the nodes in the with BFS scheme.
 *
 */
struct FlatTreeBFSRange {
  const FlatTree* tree_{nullptr};
  NodeId root_            = FlatTree::kNullNodeId;
  bool inclusive_         = true;
  bool dir_left_to_right_ = true;

  FlatTreeBFSRange(const FlatTree* tree, NodeId root, bool inclusive = true, bool dir_left_to_right = true)
      : tree_(tree), root_(root), inclusive_(inclusive), dir_left_to_right_(dir_left_to_right) {}

  FlatTreeBFSIterator begin() const { return FlatTreeBFSIterator(tree_, root_, inclusive_, dir_left_to_right_); }
  FlatTreeBFSIterator end() const { return FlatTreeBFSIterator(); }
};

}  // namespace eray::vkren
