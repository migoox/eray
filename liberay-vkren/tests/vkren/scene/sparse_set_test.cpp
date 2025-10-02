#include <gtest/gtest.h>

#include <liberay/vkren/scene/sparse_set.hpp>

using TestSparseSet = eray::vkren::SparseSet<int, std::string, double>;

TEST(SparseSetTest, InsertAndContains) {
  auto set = TestSparseSet::create(5);

  EXPECT_FALSE(set.contains_key(2));

  set.insert(2, std::string("hello"), 3.14);

  EXPECT_TRUE(set.contains_key(2));
  EXPECT_EQ(set.at<std::string>(2), "hello");
  EXPECT_DOUBLE_EQ(set.at<double>(2), 3.14);
}

TEST(SparseSetTest, MultipleInsertions) {
  auto set = TestSparseSet::create(5);

  set.insert(1, std::string("foo"), 1.1);
  set.insert(3, std::string("bar"), 2.2);
  set.insert(5, std::string("baz"), 3.3);

  EXPECT_TRUE(set.contains_key(1));
  EXPECT_TRUE(set.contains_key(3));
  EXPECT_TRUE(set.contains_key(5));

  EXPECT_EQ(set.at<std::string>(3), "bar");
  EXPECT_DOUBLE_EQ(set.at<double>(5), 3.3);
}

TEST(SparseSetTest, RemoveMiddleElement) {
  auto set = TestSparseSet::create(5);

  set.insert(1, std::string("foo"), 1.1);
  set.insert(3, std::string("bar"), 2.2);
  set.insert(5, std::string("baz"), 3.3);

  set.remove(3);

  EXPECT_FALSE(set.contains_key(3));
  EXPECT_TRUE(set.contains_key(1));
  EXPECT_TRUE(set.contains_key(5));

  EXPECT_EQ(set.at<std::string>(1), "foo");
  EXPECT_DOUBLE_EQ(set.at<double>(5), 3.3);
}

TEST(SparseSetTest, RemoveLastElement) {
  auto set = TestSparseSet::create(5);

  set.insert(1, std::string("foo"), 1.1);
  set.insert(3, std::string("bar"), 2.2);
  set.insert(5, std::string("baz"), 3.3);

  set.remove(5);

  EXPECT_TRUE(set.contains_key(3));
  EXPECT_TRUE(set.contains_key(1));
  EXPECT_FALSE(set.contains_key(5));

  EXPECT_EQ(set.at<std::string>(1), "foo");
  EXPECT_DOUBLE_EQ(set.at<double>(3), 2.2);
}

TEST(SparseSetTest, OptionalAccess) {
  auto set = TestSparseSet::create(5);

  set.insert(2, std::string("hello"), 4.2);

  auto opt_str = set.optional_at<std::string>(2);
  auto opt_dbl = set.optional_at<double>(2);

  EXPECT_TRUE(opt_str.has_value());
  EXPECT_TRUE(opt_dbl.has_value());
  EXPECT_EQ(opt_str.value(), "hello");
  EXPECT_DOUBLE_EQ(opt_dbl.value(), 4.2);

  EXPECT_FALSE(set.optional_at<std::string>(4).has_value());
}

TEST(SparseSetTest, IncreaseMaxKey) {
  auto set = TestSparseSet::create(2);
  EXPECT_EQ(set.max_key(), 2);

  set.increase_max_key(10);
  EXPECT_EQ(set.max_key(), 10);

  set.insert(10, std::string("end"), 9.9);
  EXPECT_TRUE(set.contains_key(10));
  EXPECT_EQ(set.at<std::string>(10), "end");
}

TEST(SparseSetTest, KeyValueIteration) {
  auto set = TestSparseSet::create(5);

  set.insert(1, std::string("foo"), 1.1);
  set.insert(3, std::string("bar"), 2.2);

  std::vector<int> keys;
  std::vector<std::string> strs;

  for (auto [key, value] : set.key_value_pairs<std::string>()) {
    keys.push_back(key);
    strs.push_back(value);
  }

  EXPECT_EQ(keys.size(), 2);
  EXPECT_EQ(strs.size(), 2);
  EXPECT_NE(std::ranges::find(keys, 1), keys.end());
  EXPECT_NE(std::ranges::find(keys, 3), keys.end());
  EXPECT_NE(std::ranges::find(strs, "foo"), strs.end());
  EXPECT_NE(std::ranges::find(strs, "bar"), strs.end());
}
