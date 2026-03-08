#pragma once
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace eray::vkren {

template <typename T>
concept HandleConcept = requires(typename T::IndexType i, typename T::VersionType v, T t) {
  typename T::IndexType;
  typename T::VersionType;

  { T::create(i, v) } -> std::same_as<T>;
  { t.version() } -> std::convertible_to<uint32_t>;
  { t.index() } -> std::convertible_to<uint32_t>;
};

}  // namespace eray::vkren