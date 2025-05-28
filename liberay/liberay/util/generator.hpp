#pragma once
#include <functional>
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

template <typename Container>
std::generator<typename Container::value_type> container_to_generator(
    std::reference_wrapper<const Container> container) {
  for (const auto& item : container.get()) {
    co_yield item;
  }
}

}  // namespace eray::util
