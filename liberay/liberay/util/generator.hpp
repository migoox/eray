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

template <typename T>
std::generator<T> container_to_generator(T container) {
  for (const auto& item : container) {
    co_yield item;
  }
}

}  // namespace eray::util
