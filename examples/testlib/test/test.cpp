#include "gtest/gtest.h"
#include "private/priv.hpp"

namespace {

TEST(Counter, Increment) {
  test::Counter c;

  EXPECT_EQ(-1, c.Decrement());
  EXPECT_EQ(0, c.Increment());
  EXPECT_EQ(1, c.Increment());
  EXPECT_EQ(2, c.Increment());
  EXPECT_EQ(1, c.Decrement());
}

}  // namespace