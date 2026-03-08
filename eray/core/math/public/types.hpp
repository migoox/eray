#pragma once

#include <concepts>

namespace eray::math {

template <typename T>
concept CFloatingPoint = std::floating_point<T>;

template <typename T>
concept CPrimitive = (std::integral<T> || CFloatingPoint<T>) && sizeof(T) >= 4;

}  // namespace eray::math
