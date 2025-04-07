#pragma once
#include <concepts>
#include <iterator>

namespace eray::util {

template <typename It, typename... Ts>
concept Iterator = requires(It it) {
  requires(std::convertible_to<decltype(*it), Ts> || ...);
  requires std::input_iterator<It>;
};

}  // namespace eray::util
