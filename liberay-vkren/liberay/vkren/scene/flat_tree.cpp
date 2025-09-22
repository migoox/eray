#include <cassert>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <ranges>
#include <stack>
#include <vector>

namespace eray::vkren {

FlatTree::NodeId FlatTree::create_node(NodeId parent_id) {
  auto parent_index = extract_node_index(parent_id);
  assert(parent_index != kNullNodeIndex && "Provided parent must not be null");

  auto node_index = free_nodes_.back();
  free_nodes_.pop_back();

  nodes_[node_index] = Node{
      .parent        = parent_index,
      .right_child   = kNullNodeIndex,
      .left_sibling  = nodes_[node_index].right_child,
      .right_sibling = kNullNodeIndex,
  };
  level_[node_index] = level_[parent_index] + 1;

  nodes_[parent_index].right_child = node_index;

  is_dirty_ = true;
  ++node_count_;

  return compose_node_id(node_index, version_[node_index]);
}

const FlatTree::Node& FlatTree::node_info(NodeId node_id) const {
  auto index = extract_node_index(node_id);
  return nodes_[index];
}

void FlatTree::delete_node(NodeId node_id) {
  auto node_index = extract_node_index(node_id);
  assert(node_index != kNullNodeIndex && "Provided node must not be null");
  assert(node_index != kRootNodeIndex && "Root node must not be deleted");

  free_nodes_.push_back(node_index);

  auto left   = nodes_[node_index].left_sibling;
  auto right  = nodes_[node_index].right_sibling;
  auto parent = nodes_[node_index].parent;

  nodes_[left].right_sibling = right;
  nodes_[right].left_sibling = left;
  if (nodes_[parent].right_child == node_index) {
    nodes_[parent].right_child = nodes_[node_index].left_sibling;
  }

  --node_count_;
  is_dirty_ = true;
  version_[node_index]++;
}

void FlatTree::make_orphan(NodeId node_id) { change_parent(node_id, kRootNodeId); }

void FlatTree::change_parent(NodeId node_id, NodeId parent_id) {
  auto node_index   = extract_node_index(node_id);
  auto parent_index = extract_node_index(parent_id);
  assert(node_index != kNullNodeIndex && "Provided node must not be null");
  assert(node_index != kRootNodeIndex && "Root node must not be deleted");

  auto left   = nodes_[node_index].left_sibling;
  auto right  = nodes_[node_index].right_sibling;
  auto parent = nodes_[node_index].parent;

  nodes_[left].right_sibling = right;
  nodes_[right].left_sibling = left;
  if (nodes_[parent].right_child == node_index) {
    nodes_[parent].right_child = nodes_[node_index].left_sibling;
  }

  nodes_[node_index].parent        = parent_index;
  nodes_[node_index].right_sibling = kNullNodeIndex;
  nodes_[node_index].left_sibling  = kNullNodeIndex;

  auto right_child = nodes_[parent_index].right_child;
  if (right_child != kNullNodeIndex) {
    nodes_[right_child].right_sibling = node_index;
    nodes_[node_index].left_sibling   = right_child;
  }
  nodes_[parent_index].right_child = node_index;

  is_dirty_ = true;
}

uint32_t FlatTree::node_level(NodeId node_id) const {
  assert(node_id != kNullNodeId && "Node must not be null");
  auto node_index = extract_node_index(node_id);
  return level_[node_index];
}

const std::vector<FlatTree::NodeId>& FlatTree::nodes_bfs_order() const {
  if (!is_dirty_) {
    return bfs_order_cached_;
  }

  bfs_order_cached_.clear();
  if (node_count_ == 0) {
    return bfs_order_cached_;
  }

  bfs_order_cached_.reserve(node_count_);
  bfs_order_cached_.push_back(kRootNodeId);

  size_t pivot = 0;
  while (pivot < bfs_order_cached_.size()) {
    auto curr_node = bfs_order_cached_[pivot++];
    auto child     = nodes_[curr_node].right_child;
    while (child != kNullNodeId) {
      bfs_order_cached_.push_back(index2id(child));
      child = nodes_[child].left_sibling;
    }
  }

  is_dirty_ = false;

  return bfs_order_cached_;
}

const std::vector<FlatTree::NodeId>& FlatTree::nodes_dfs_preorder() const {
  if (!is_dirty_) {
    return dfs_preorder_cached_;
  }

  dfs_preorder_cached_.clear();
  if (node_count_ == 0) {
    return dfs_preorder_cached_;
  }

  dfs_preorder_cached_.reserve(node_count_);

  std::stack<NodeIndex> stack;
  stack.push(kRootNodeId);
  while (!stack.empty()) {
    auto curr_node = stack.top();
    stack.pop();
    dfs_preorder_cached_.push_back(index2id(curr_node));

    auto child = nodes_[curr_node].right_child;
    while (child != kNullNodeIndex) {
      stack.push(child);
      child = nodes_[child].left_sibling;
    }
  }

  is_dirty_ = false;

  return dfs_preorder_cached_;
}

FlatTree FlatTree::create(size_t max_nodes_count) {
  auto tree = FlatTree();
  tree.nodes_.resize(max_nodes_count, Node{});
  tree.level_.resize(max_nodes_count, 0);
  tree.version_.resize(max_nodes_count, 0);
  tree.free_nodes_ = std::views::iota(0U, max_nodes_count) | std::views::reverse | std::ranges::to<std::vector>();
  tree.node_count_ = 0;

  return tree;
}

bool FlatTree::exists(NodeId node_id) const {
  assert(node_id != kNullNodeId && "Node must not be null");
  auto [index, version] = decompose_node_id(node_id);
  return version_[index] == version;
}

}  // namespace eray::vkren
