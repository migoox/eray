#include <gtest/gtest.h>

#include <liberay/vkren/scene/entity_pool.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>

using FlatTree = eray::vkren::FlatTree;
using NodeId   = eray::vkren::NodeId;

TEST(FlatTreeTest, RootNodeExists) {
  FlatTree tree = FlatTree::create(10);
  EXPECT_TRUE(tree.exists(FlatTree::kRootNodeId));
}

TEST(FlatTreeTest, CreateNodeDefaultParent) {
  FlatTree tree = FlatTree::create(10);
  NodeId n      = tree.create_node();
  EXPECT_TRUE(tree.exists(n));
  EXPECT_EQ(tree.parent_of(n), FlatTree::kRootNodeId);
}

TEST(FlatTreeTest, CreateNodeWithParent) {
  FlatTree tree = FlatTree::create(10);
  NodeId parent = tree.create_node();
  NodeId child  = tree.create_node(parent);
  EXPECT_EQ(tree.parent_of(child), parent);
}

TEST(FlatTreeTest, SiblingsAreCorrect) {
  FlatTree tree = FlatTree::create(10);
  NodeId first  = tree.create_node();
  NodeId second = tree.create_node();
  NodeId third  = tree.create_node();

  EXPECT_EQ(tree.left_sibling_of(second), first);
  EXPECT_EQ(tree.right_sibling_of(first), second);
  EXPECT_EQ(tree.left_sibling_of(third), second);
  EXPECT_EQ(tree.right_sibling_of(second), third);
}

TEST(FlatTreeTest, NodeLevel) {
  FlatTree tree = FlatTree::create(10);
  NodeId n1     = tree.create_node();
  NodeId n2     = tree.create_node(n1);
  NodeId n3     = tree.create_node(n2);

  EXPECT_EQ(tree.node_level(n1), 1);
  EXPECT_EQ(tree.node_level(n2), 2);
  EXPECT_EQ(tree.node_level(n3), 3);
}

TEST(FlatTreeTest, CopyNode) {
  FlatTree tree   = FlatTree::create(10);
  NodeId parent   = tree.create_node();
  NodeId child    = tree.create_node(parent);
  NodeId child2_1 = tree.create_node(child);
  NodeId child2_2 = tree.create_node(child);

  NodeId copy = tree.copy_node(child, FlatTree::kRootNodeId);
  EXPECT_EQ(tree.parent_of(copy), FlatTree::kRootNodeId);
  EXPECT_NE(copy, child);

  auto ids = std::vector<NodeId>();
  for (auto c : eray::vkren::FlatTreeDFSRange(&tree, copy, false)) {
    ids.push_back(c);
  }

  EXPECT_EQ(ids.size(), 2);
  EXPECT_EQ(std::ranges::find(ids, child2_1), ids.end());
  EXPECT_EQ(std::ranges::find(ids, child2_2), ids.end());
}

TEST(FlatTreeTest, CopyAndPasteAsChild) {
  FlatTree tree   = FlatTree::create(11);
  NodeId parent   = tree.create_node();
  NodeId child    = tree.create_node(parent);
  NodeId child2_1 = tree.create_node(child);
  NodeId child2_2 = tree.create_node(child);

  NodeId copy = tree.copy_node(child, child);
  EXPECT_EQ(tree.parent_of(copy), child);

  auto ids = std::vector<NodeId>();
  for (auto c : eray::vkren::FlatTreeDFSRange(&tree, copy, false)) {
    ids.push_back(c);
  }

  EXPECT_EQ(ids.size(), 2);
  EXPECT_EQ(std::ranges::find(ids, child2_1), ids.end());
  EXPECT_EQ(std::ranges::find(ids, child2_2), ids.end());
}

TEST(FlatTreeTest, DeleteNode) {
  FlatTree tree = FlatTree::create(10);
  NodeId parent = tree.create_node();
  NodeId child1 = tree.create_node(parent);
  NodeId child2 = tree.create_node(parent);

  tree.delete_node(parent);
  EXPECT_FALSE(tree.exists(parent));
  EXPECT_FALSE(tree.exists(child1));
  EXPECT_FALSE(tree.exists(child2));
}

TEST(FlatTreeTest, MakeOrphan) {
  FlatTree tree = FlatTree::create(10);
  NodeId n      = tree.create_node();
  NodeId parent = tree.create_node();
  tree.change_parent(n, parent);

  tree.make_orphan(n);
  EXPECT_EQ(tree.parent_of(n), FlatTree::kRootNodeId);
}

TEST(FlatTreeTest, BFSIteration) {
  FlatTree tree = FlatTree::create(10);
  NodeId n1     = tree.create_node();
  NodeId n2     = tree.create_node();
  NodeId n3     = tree.create_node(n1);
  NodeId n4     = tree.create_node(n1);

  tree.delete_node(n2);

  std::vector<NodeId> ids;
  for (auto id : eray::vkren::FlatTreeBFSRange(&tree, FlatTree::kRootNodeId)) {
    ids.push_back(id);
  }

  EXPECT_EQ(ids.front(), FlatTree::kRootNodeId);
  EXPECT_NE(std::ranges::find(ids, n1), ids.end());
  EXPECT_EQ(std::ranges::find(ids, n2), ids.end());
  EXPECT_NE(std::ranges::find(ids, n3), ids.end());
  EXPECT_NE(std::ranges::find(ids, n4), ids.end());
}

TEST(FlatTreeTest, DFSIteration) {
  FlatTree tree = FlatTree::create(10);
  NodeId n1     = tree.create_node();
  NodeId n2     = tree.create_node();
  NodeId n3     = tree.create_node(n1);
  NodeId n4     = tree.create_node(n1);

  tree.delete_node(n2);

  std::vector<NodeId> ids;
  for (auto id : eray::vkren::FlatTreeDFSRange(&tree, FlatTree::kRootNodeId)) {
    ids.push_back(id);
  }

  EXPECT_EQ(ids.front(), FlatTree::kRootNodeId);
  EXPECT_NE(std::ranges::find(ids, n1), ids.end());
  EXPECT_EQ(std::ranges::find(ids, n2), ids.end());
  EXPECT_NE(std::ranges::find(ids, n3), ids.end());
  EXPECT_NE(std::ranges::find(ids, n4), ids.end());
}

TEST(FlatTreeTest, Exists) {
  FlatTree tree = FlatTree::create(10);
  NodeId n      = tree.create_node();
  EXPECT_TRUE(tree.exists(n));
  tree.delete_node(n);
  EXPECT_FALSE(tree.exists(n));
}

TEST(FlatTreeTest, NodeSurroundingInfo) {
  FlatTree tree = FlatTree::create(10);
  NodeId a      = tree.create_node();
  NodeId b      = tree.create_node();
  NodeId c      = tree.create_node(a);

  eray::vkren::NodeSurroundingInfo info = tree.node_surrounding_info(c);
  EXPECT_EQ(info.parent_id, a);
  EXPECT_EQ(info.left_sibling_id, FlatTree::kNullNodeId);
}
