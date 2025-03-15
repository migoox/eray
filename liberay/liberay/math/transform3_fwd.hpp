#pragma once
#include <liberay/math/types.hpp>

namespace eray::math {

template <CFloatingPoint T>
struct Transform3;

using Transform3f  = Transform3<float>;
using Transformd3d = Transform3<double>;

}  // namespace eray::math
