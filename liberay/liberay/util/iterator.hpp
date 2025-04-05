#pragma once
#include <concepts>
#include <iterator>

namespace eray::util {

template <typename It, typename T>
concept Iterator = requires(It it) {
  { *it } -> std::convertible_to<T>;
  requires std::input_iterator<It>;
};

}  // namespace eray::util
