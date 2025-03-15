#pragma once
#include <cstdint>
#include <liberay/math/types.hpp>

namespace eray::math {

template <std::size_t N, CPrimitive T>
  requires(N >= 1)
struct Vec;

using Vec2f = Vec<2, float>;
using Vec3f = Vec<3, float>;
using Vec4f = Vec<4, float>;

using Vec2d = Vec<2, double>;
using Vec3d = Vec<3, double>;
using Vec4d = Vec<4, double>;

using Vec2i = Vec<2, int>;
using Vec3i = Vec<3, int>;
using Vec4i = Vec<4, int>;

using Vec2u = Vec<2, uint32_t>;
using Vec3u = Vec<3, uint32_t>;
using Vec4u = Vec<4, uint32_t>;

}  // namespace eray::math
