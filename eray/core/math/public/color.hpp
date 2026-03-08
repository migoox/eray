#pragma once
#include <liberay/math/types.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>

namespace eray::math {

template <CFloatingPoint T>
Vec3<T> hsv2rgb(Vec3<T> hsv) {
  Vec3<T> rgb =
      clamp(abs(mod(hsv.x * static_cast<T>(6) + Vec3<T>{static_cast<T>(0), static_cast<T>(4), static_cast<T>(2)},
                    static_cast<T>(6)) -
                static_cast<T>(3)) -
                static_cast<T>(1),
            static_cast<T>(0), static_cast<T>(1));
  return hsv.z * mix(Vec3<T>{static_cast<T>(1), static_cast<T>(1), static_cast<T>(1)}, rgb, hsv.y);
}

}  // namespace eray::math
