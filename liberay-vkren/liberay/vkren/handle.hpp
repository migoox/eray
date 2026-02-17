#pragma once
#include <concepts>
#include <cstdint>
#include <type_traits>

namespace eray::vkren {

template <typename T>
concept HandleConcept = requires(T t) {
  typename T::IndexType;
  typename T::VersionType;
  { T::create() } -> HandleConcept;
  { t.version() } -> std::convertible_to<uint32_t>;
  { t.index() } -> std::convertible_to<uint32_t>;
};

}  // namespace eray::vkren