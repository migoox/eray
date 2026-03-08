#include <gtest/gtest.h>

#include <liberay/vkren/scene/entity_pool.hpp>

struct TestTag {};
using TestEntityId   = eray::vkren::EntityId<TestTag>;
using TestEntityPool = eray::vkren::EntityPool<TestEntityId>;

TEST(BasicObjectPoolTest, CreatePool) {
  auto pool = TestEntityPool::create(5);
  EXPECT_EQ(pool.count(), 0);  // Pool starts empty
}

TEST(BasicObjectPoolTest, CreateEntities) {
  auto pool = TestEntityPool::create(3);

  auto id1 = pool.create();
  auto id2 = pool.create();
  auto id3 = pool.create();

  EXPECT_NE(id1.value, id2.value);
  EXPECT_NE(id2.value, id3.value);
  EXPECT_NE(id1.value, id3.value);

  EXPECT_EQ(pool.count(), 3);
  EXPECT_TRUE(pool.exists(id1));
  EXPECT_TRUE(pool.exists(id2));
  EXPECT_TRUE(pool.exists(id3));
}

TEST(BasicObjectPoolTest, RemoveEntities) {
  auto pool = TestEntityPool::create(2);

  auto id1 = pool.create();
  auto id2 = pool.create();

  EXPECT_EQ(pool.count(), 2);

  pool.remove(id1);
  EXPECT_EQ(pool.count(), 1);
  EXPECT_FALSE(pool.exists(id1));
  EXPECT_TRUE(pool.exists(id2));

  // Removing id2
  pool.remove(id2);
  EXPECT_EQ(pool.count(), 0);
  EXPECT_FALSE(pool.exists(id2));
}

TEST(BasicObjectPoolTest, EntitiesWithDifferentVersionsNotEqual) {
  auto pool = TestEntityPool::create(2);

  auto id1 = pool.create();
  pool.remove(id1);
  auto id1_new = pool.create();
  EXPECT_NE(id1.value, id1_new.value);
}

TEST(BasicObjectPoolTest, ReuseRemovedEntities) {
  auto pool = TestEntityPool::create(1);

  auto id1 = pool.create();
  pool.remove(id1);

  auto id2 = pool.create();
  EXPECT_NE(id1.value, id2.value);                                          // Version should be incremented
  EXPECT_EQ(TestEntityPool::index_of(id1), TestEntityPool::index_of(id2));  // Same index reused
  EXPECT_EQ(TestEntityPool::version_of(id2), TestEntityPool::version_of(id1) + 1);
}

TEST(BasicObjectPoolTest, ComposeAndIndexVersion) {
  size_t index     = 42;
  uint32_t version = 7;
  auto id          = TestEntityPool::compose_id(index, version);

  EXPECT_EQ(TestEntityPool::index_of(id), index);
  EXPECT_EQ(TestEntityPool::version_of(id), version);
}

TEST(BasicObjectPoolTest, ComposeIdWithPool) {
  auto pool     = TestEntityPool::create(3);
  auto id       = pool.create();
  size_t index  = TestEntityPool::index_of(id);
  auto composed = pool.compose_id(index);
  EXPECT_EQ(composed.value, id.value);  // Compose with pool version
}
