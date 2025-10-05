#include <algorithm>
#include <cassert>
#include <liberay/vkren/scene/entity_pool.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <optional>
#include <stack>
#include <unordered_map>
#include <vector>

namespace eray::vkren {

NodeId FlatTree::create_node(NodeId parent_id) {
  auto parent_index = EntityPool<NodeId>::index_of(parent_id);

  assert(parent_index != kNullNodeIndex && "Provided parent must not be null");
  assert(exists(parent_id) && "Parent must exist");

  auto new_node_id    = nodes_pool_.create();
  auto new_node_index = EntityPool<NodeId>::index_of(new_node_id);

  nodes_[new_node_index] = Node{
      .parent        = parent_index,
      .left_child    = kNullNodeIndex,
      .right_child   = kNullNodeIndex,
      .left_sibling  = nodes_[parent_index].right_child,
      .right_sibling = kNullNodeIndex,
  };

  if (nodes_[parent_index].right_child != kNullNodeIndex) {
    nodes_[nodes_[parent_index].right_child].right_sibling = new_node_index;
  }
  if (nodes_[parent_index].left_child == kNullNodeIndex) {
    nodes_[parent_index].left_child = new_node_index;
  }
  nodes_[parent_index].right_child = new_node_index;

  level_[new_node_index] = level_[parent_index] + 1;

  set_dirty();

  return new_node_id;
}

NodeId FlatTree::parent_of(NodeId node_id) const {
  assert(node_id != kNullNodeId && "Node must not be null");

  auto index = EntityPool<NodeId>::index_of(node_id);
  assert(nodes_[index].parent != kNullNodeIndex && "Parent must never be null");

  return nodes_pool_.compose_id(nodes_[index].parent);
}

std::optional<NodeId> FlatTree::left_sibling_of(NodeId node_id) const {
  assert(node_id != kNullNodeId && "Node must not be null");
  if (node_id == kNullNodeId) {
    return std::nullopt;
  }

  auto index = EntityPool<NodeId>::index_of(node_id);
  if (nodes_[index].left_sibling == kNullNodeIndex) {
    return std::nullopt;
  }

  return compose_id(nodes_[index].left_sibling);
}

std::optional<NodeId> FlatTree::right_sibling_of(NodeId node_id) const {
  assert(node_id != kNullNodeId && "Node must not be null");
  if (node_id == kNullNodeId) {
    return std::nullopt;
  }

  auto index = EntityPool<NodeId>::index_of(node_id);
  if (nodes_[index].right_sibling == kNullNodeIndex) {
    return std::nullopt;
  }

  return compose_id(nodes_[index].right_sibling);
}

NodeSurroundingInfo FlatTree::node_surrounding_info(NodeId node_id) const {
  assert(node_id != kNullNodeId && "Node must not be null");

  auto index       = EntityPool<NodeId>::index_of(node_id);
  const auto& node = nodes_[index];
  return NodeSurroundingInfo{
      .parent_id        = nodes_pool_.compose_id(node.parent),
      .left_child_id    = nodes_pool_.compose_id(node.left_child),
      .right_child_id   = nodes_pool_.compose_id(node.right_child),
      .left_sibling_id  = nodes_pool_.compose_id(node.left_sibling),
      .right_sibling_id = nodes_pool_.compose_id(node.right_sibling),
  };
}

void FlatTree::delete_node(NodeId node_id) {
  auto node_index = EntityPool<NodeId>::index_of(node_id);
  assert(node_index != kRootNodeIndex && "Root node must not be deleted");
  assert(exists(node_id) && "Node must exist");

  auto left   = nodes_[node_index].left_sibling;
  auto right  = nodes_[node_index].right_sibling;
  auto parent = nodes_[node_index].parent;

  if (nodes_[parent].left_child == nodes_[parent].right_child) {
    nodes_[parent].left_child = nodes_[parent].right_child = kNullNodeIndex;
  } else if (node_index == nodes_[parent].left_child) {
    nodes_[parent].left_child = right;
  } else if (node_index == nodes_[parent].right_child) {
    nodes_[parent].right_child = left;
  }

  if (left != kNullNodeIndex) {
    nodes_[left].right_sibling = right;
  }
  if (right != kNullNodeIndex) {
    nodes_[right].left_sibling = left;
  }

  set_dirty();

  // Delete all descendants
  for (auto descendant_id : FlatTreeDFSRange(this, node_id)) {
    nodes_pool_.remove(descendant_id);
  }
}

NodeId FlatTree::copy_node(NodeId node_id, NodeId parent_id) {
  assert(exists(node_id) && "Node must exist");
  assert(exists(parent_id) && "Parent must exist");

  auto old_to_new     = std::unordered_map<NodeId, NodeId>();
  old_to_new[node_id] = create_node();  // use root and later change parent, this handles node_id == parent_id case

  for (auto curr_node_id : FlatTreeBFSRange(this, node_id, false)) {
    auto curr_node_index     = EntityPool<NodeId>::index_of(curr_node_id);
    auto curr_parent_id      = nodes_pool_.compose_id(nodes_[curr_node_index].parent);
    old_to_new[curr_node_id] = create_node(old_to_new[curr_parent_id]);
  }

  change_parent(old_to_new[node_id], parent_id);

  set_dirty();

  return old_to_new[node_id];
}

void FlatTree::make_orphan(NodeId node_id) { change_parent(node_id, kRootNodeId); }

void FlatTree::change_parent(NodeId node_id, NodeId parent_id) {
  auto node_index   = EntityPool<NodeId>::index_of(node_id);
  auto parent_index = EntityPool<NodeId>::index_of(parent_id);

  assert(node_index != kNullNodeIndex && "Provided node must not be null");
  assert(node_index != kRootNodeIndex && "Root node must not be deleted");
  assert(exists(parent_id) && "Parent must exist");

  auto left   = nodes_[node_index].left_sibling;
  auto right  = nodes_[node_index].right_sibling;
  auto parent = nodes_[node_index].parent;

  if (nodes_[parent].left_child == nodes_[parent].right_child) {
    nodes_[parent].left_child = nodes_[parent].right_child = kNullNodeIndex;
  } else if (node_index == nodes_[parent].left_child) {
    nodes_[parent].left_child = right;
  } else if (node_index == nodes_[parent].right_child) {
    nodes_[parent].right_child = left;
  }

  if (left != kNullNodeIndex) {
    nodes_[left].right_sibling = right;
  }
  if (right != kNullNodeIndex) {
    nodes_[right].left_sibling = left;
  }

  nodes_[node_index].parent        = parent_index;
  nodes_[node_index].right_sibling = kNullNodeIndex;
  nodes_[node_index].left_sibling  = kNullNodeIndex;

  level_[node_index] = level_[parent_index] + 1;

  if (nodes_[parent_index].right_child != kNullNodeIndex) {
    nodes_[nodes_[parent_index].right_child].right_sibling = node_index;
  }
  if (nodes_[parent_index].left_child == kNullNodeIndex) {
    nodes_[parent_index].left_child = node_index;
  }
  nodes_[parent_index].right_child = node_index;

  set_dirty();
}

uint32_t FlatTree::node_level(NodeId node_id) const {
  assert(exists(node_id) && "Node must exist");
  auto node_index = EntityPool<NodeId>::index_of(node_id);
  return level_[node_index];
}

const std::vector<NodeId>& FlatTree::nodes_bfs_order() const {
  if (!bfs_dirty_) {
    return bfs_order_cached_;
  }

  bfs_order_cached_.clear();
  if (nodes_pool_.count() == 0) {
    return bfs_order_cached_;
  }

  bfs_order_cached_.reserve(nodes_pool_.count());
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
  if (nodes_pool_.count() == 0) {
    return dfs_preorder_cached_;
  }

  dfs_preorder_cached_.reserve(nodes_pool_.count());
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
  tree.nodes_pool_ = EntityPool<NodeId>::create(max_nodes_count);
  auto root_id     = tree.nodes_pool_.create();
  assert(root_id == kRootNodeId && "Root index invariant not met");
  return tree;
}

bool FlatTree::is_descendant(NodeId node_id, NodeId ancestor_id) const {
  assert(exists(node_id) && "Node must not be null");
  assert(exists(ancestor_id) && "Ancestor node must not be null");

  if (node_id == ancestor_id) {
    return false;
  }

  std::ranges::any_of(FlatTreeBFSRange(this, ancestor_id, false),
                      [node_id](auto& descendant_id) { return descendant_id == node_id; });

  return false;
}

bool FlatTree::exists(NodeId node_id) const { return node_id != kNullNodeId && nodes_pool_.exists(node_id); }

void FlatTree::set_dirty() {
  dfs_dirty_ = true;
  bfs_dirty_ = true;
}

FlatTreeDFSIterator::FlatTreeDFSIterator(const FlatTree* tree, NodeId start, bool inclusive)
    : tree_(tree), current_(FlatTree::kNullNodeId) {
  if (tree_ && tree_->exists(start)) {
    stack_.push(start);
    advance();
    if (!inclusive) {
      advance();
    }
  }
}

void FlatTreeDFSIterator::advance() {
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
  auto child = tree_->nodes_[EntityPool<NodeId>::index_of(current_)].right_child;
  while (child != FlatTree::kNullNodeIndex) {
    stack_.push(tree_->nodes_pool_.compose_id(child));
    child = tree_->nodes_[child].left_sibling;
  }
}

FlatTreeBFSIterator::FlatTreeBFSIterator(const FlatTree* tree, NodeId start, bool inclusive, bool dir_left_to_right)
    : tree_(tree), current_(FlatTree::kNullNodeId), dir_left_to_right_(dir_left_to_right) {
  if (tree_ && tree_->exists(start)) {
    queue_.push(start);
    advance();
    if (!inclusive) {
      advance();
    }
  }
}

void FlatTreeBFSIterator::advance() {
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
  if (dir_left_to_right_) {
    auto child = tree_->nodes_[EntityPool<NodeId>::index_of(current_)].left_child;
    while (child != FlatTree::kNullNodeIndex) {
      queue_.push(tree_->nodes_pool_.compose_id(child));
      child = tree_->nodes_[child].right_sibling;
    }
  } else {
    auto child = tree_->nodes_[EntityPool<NodeId>::index_of(current_)].right_child;
    while (child != FlatTree::kNullNodeIndex) {
      queue_.push(tree_->nodes_pool_.compose_id(child));
      child = tree_->nodes_[child].left_sibling;
    }
  }
}

}  // namespace eray::vkren
