#include "private/priv.hpp"

#include <print>

#include "public/testlib/pub.hpp"

namespace test {

void init() {
  Counter counter;
  counter.Print();
};

int Counter::Increment() { return ++counter_; }

int Counter::Decrement() { return --counter_; }

void Counter::Print() const { std::println("Counter value: {}", counter_); }

}  // namespace test