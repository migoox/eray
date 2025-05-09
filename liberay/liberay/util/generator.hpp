#pragma once
#include <generator>

namespace eray::util {
template <typename T>
struct SizedGenerator {
  SizedGenerator() = delete;
  SizedGenerator(std::generator<T>&& gen, size_t size);

  std::generator<T> gen;
  size_t size;

  auto begin() { return gen.begin(); }
  auto end() { return gen.end(); }
};

}  // namespace eray::util
