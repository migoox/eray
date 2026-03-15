#pragma once

namespace test {
struct PrivateStruct {
	float t;
};
// A simple monotonic counter.
class Counter {
 private:
  int counter_;

 public:
  Counter() : counter_(0) {}

  int Increment();
  int Decrement();
  void Print() const;
};
}  // namespace test