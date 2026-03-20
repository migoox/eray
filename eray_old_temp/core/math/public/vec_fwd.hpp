#pragma once
#include <cstdint>
#include <liberay/math/types.hpp>

namespace eray::math {

template <std::size_t N, CPrimitive T>
  requires(N >= 1)
struct Vec;

template <CPrimitive T>
using Vec2 = Vec<2, T>;

template <CPrimitive T>
using Vec3 = Vec<3, T>;

template <CPrimitive T>
using Vec4 = Vec<4, T>;

using Vec2i = Vec2<int>;
using Vec2u = Vec2<uint32_t>;
using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;

using Vec3i = Vec3<int>;
using Vec3u = Vec3<uint32_t>;
using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;

using Vec4i = Vec4<int>;
using Vec4u = Vec4<uint32_t>;
using Vec4f = Vec4<float>;
using Vec4d = Vec4<double>;

}  // namespace eray::math
