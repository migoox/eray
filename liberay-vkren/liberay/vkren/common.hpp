#pragma once

#include <liberay/util/result.hpp>

namespace eray::vkren {

template <typename T, typename E>
using Result = eray::util::Result<T, E>;

template <typename TFlags, typename TFlagBits>
constexpr bool has_flag(TFlags lhs, TFlagBits rhs) {
  return (lhs & rhs) == rhs;
}

}  // namespace eray::vkren
