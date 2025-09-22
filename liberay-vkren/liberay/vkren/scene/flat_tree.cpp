#include <cassert>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <ranges>
#include <stack>
#include <unordered_map>
#include <vector>

namespace eray::vkren {

NodeId FlatTree::create_node(NodeId parent_id) {
  auto parent_index = node_index_of(parent_id);
  assert(parent_index != kNullNodeIndex && "Provided parent must not be null");
  assert(exists(parent_index) && "Parent must exist");

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

  set_dirty();
  ++node_count_;

  return compose_node_id(node_index, version_[node_index]);
}

NodeId FlatTree::parent_of(NodeId node_id) const {
  auto index = node_index_of(node_id);
  return index_to_id(nodes_[index].parent);
}
NodeId FlatTree::left_sibling_of(NodeId node_id) const {
  auto index = node_index_of(node_id);
  return index_to_id(nodes_[index].left_sibling);
}
NodeId FlatTree::right_sibling_of(NodeId node_id) const {
  auto index = node_index_of(node_id);
  return index_to_id(nodes_[index].right_sibling);
}

const FlatTree::Node& FlatTree::node_info(NodeId node_id) const {
  auto index = node_index_of(node_id);
  return nodes_[index];
}

void FlatTree::delete_node(NodeId node_id) {
  auto node_index = node_index_of(node_id);
  assert(node_index != kNullNodeIndex && "Provided node must not be null");
  assert(node_index != kRootNodeIndex && "Root node must not be deleted");
  assert(exists(node_id) && "Node must exist");

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
  set_dirty();

  // Delete all descendants
  for (auto descendant : FlatTreeDFSRange(this, node_id)) {
    version_[node_index_of(descendant)]++;
  }
}

NodeId FlatTree::copy_node(NodeId node_id, NodeId parent_id) {
  auto node_index   = node_index_of(node_id);
  auto parent_index = node_index_of(parent_id);
  assert(node_index != kNullNodeIndex && "Provided node must not be null");
  assert(parent_index != kNullNodeIndex && "Parent must not be null");
  assert(exists(node_id) && "Node must exist");
  assert(exists(parent_id) && "Parent must exist");

  auto old_to_new        = std::unordered_map<NodeId, NodeId>();
  old_to_new[node_index] = create_node(parent_id);

  for (auto node : FlatTreeBFSRange(this, node_id, false)) {
    auto curr_node_index        = node_index_of(node);
    auto curr_parent_id         = index_to_id(nodes_[curr_node_index].parent);
    old_to_new[curr_node_index] = create_node(old_to_new[curr_parent_id]);
  }

  set_dirty();

  return old_to_new[node_id];
}

void FlatTree::make_orphan(NodeId node_id) { change_parent(node_id, kRootNodeId); }

void FlatTree::change_parent(NodeId node_id, NodeId parent_id) {
  auto node_index   = node_index_of(node_id);
  auto parent_index = node_index_of(parent_id);
  assert(node_index != kNullNodeIndex && "Provided node must not be null");
  assert(node_index != kRootNodeIndex && "Root node must not be deleted");
  assert(exists(parent_id) && "Parent must exist");

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

  set_dirty();
}

uint32_t FlatTree::node_level(NodeId node_id) const {
  assert(node_id != kNullNodeId && "Node must not be null");
  auto node_index = node_index_of(node_id);
  return level_[node_index];
}

const std::vector<NodeId>& FlatTree::nodes_bfs_order() const {
  if (!bfs_dirty_) {
    return bfs_order_cached_;
  }

  bfs_order_cached_.clear();
  if (node_count_ == 0) {
    return bfs_order_cached_;
  }

  bfs_order_cached_.reserve(node_count_);
  for (auto node : FlatTreeBFSRange(this, kRootNodeId)) {
    dfs_preorder_cached_.push_back(node);
  }
  bfs_dirty_ = false;

  return bfs_order_cached_;
}

const std::vector<NodeId>& FlatTree::nodes_dfs_preorder() const {
  if (!dfs_dirty_) {
    return dfs_preorder_cached_;
  }

  dfs_preorder_cached_.clear();
  if (node_count_ == 0) {
    return dfs_preorder_cached_;
  }

  dfs_preorder_cached_.reserve(node_count_);
  for (auto node : FlatTreeDFSRange(this, kRootNodeId)) {
    dfs_preorder_cached_.push_back(node);
  }
  dfs_dirty_ = false;
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

void FlatTree::set_dirty() {
  dfs_dirty_ = true;
  bfs_dirty_ = true;
}

FlatTreeDFSIterator::FlatTreeDFSIterator(const FlatTree* tree, NodeId start, bool inclusive)
    : tree_(tree), current_(start) {
  if (tree_ && tree_->exists(start)) {
    stack_.push(start);
    if (!inclusive) {
      advance();
    }
  }
}

void FlatTreeDFSIterator::advance() {
  assert(current_ != FlatTree::kNullNodeId && "Node must not be null");
  if (stack_.empty()) {
    current_ = FlatTree::kNullNodeId;
    return;
  }

  // acquire the current
  current_ = stack_.top();
  stack_.pop();

  if (!tree_->exists(current_)) {
    current_ = FlatTree::kNullNodeId;
    return;
  }

  // push children
  auto child = tree_->node_info(current_).right_child;
  while (child != FlatTree::kNullNodeIndex) {
    stack_.push(tree_->index_to_id(child));
    child = tree_->node_info(child).left_sibling;
  }
}

FlatTreeBFSIterator::FlatTreeBFSIterator(const FlatTree* tree, NodeId start, bool inclusive)
    : tree_(tree), current_(start) {
  if (tree_ && tree_->exists(start)) {
    queue_.push(start);
    if (!inclusive) {
      advance();
    }
  }
}

void FlatTreeBFSIterator::advance() {
  assert(current_ != FlatTree::kNullNodeId && "Node must not be null");
  if (queue_.empty()) {
    current_ = FlatTree::kNullNodeId;
    return;
  }

  // acquire the current
  current_ = queue_.front();
  queue_.pop();

  if (!tree_->exists(current_)) {
    current_ = FlatTree::kNullNodeId;
    return;
  }

  // push children
  auto child = tree_->node_info(current_).right_child;
  while (child != FlatTree::kNullNodeIndex) {
    queue_.push(tree_->index_to_id(child));
    child = tree_->node_info(child).left_sibling;
  }
}

}  // namespace eray::vkren
