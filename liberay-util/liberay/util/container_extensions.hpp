#pragma once
#include <array>

namespace eray::util {

namespace internal {

template <typename T, std::size_t N, std::size_t... Is>
constexpr std::array<T, N> make_filled_array_impl(const T& value, std::index_sequence<Is...>) {
  return {((void)Is, value)...};
}

}  // namespace internal

template <typename T, std::size_t N>
constexpr std::array<T, N> make_filled_array(const T& value) {
  return internal::make_filled_array_impl<T, N>(value, std::make_index_sequence<N>{});
}

}  // namespace eray::util
