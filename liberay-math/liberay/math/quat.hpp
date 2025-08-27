#pragma once

#include <cassert>
#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/quat_fwd.hpp>
#include <liberay/math/types.hpp>

namespace eray::math {

template <CFloatingPoint T>
struct Quat {
 public:
  T w, x, y, z;

  // CONSTRUCTORS ////////////////////////////////////////////////////////////////////////////////////

  constexpr explicit Quat(const Vec<4, T>& vec) : w(vec.w), x(vec.x), y(vec.y), z(vec.z) {}
  constexpr explicit Quat(T real, const Vec<3, T>& imaginary)
      : w(real), x(imaginary.x), y(imaginary.y), z(imaginary.z) {}
  constexpr explicit Quat(const Vec<3, T>& imaginary)
      : w(static_cast<T>(1)), x(imaginary.x()), y(imaginary.y()), z(imaginary.z()) {}
  constexpr explicit Quat(T val) : w(val), x(val), y(val), z(val) {}
  constexpr explicit Quat(T _w, T _x, T _y, T _z) : w(_w), x(_x), y(_y), z(_z) {}
  constexpr explicit Quat() : w(static_cast<T>(1)), x(static_cast<T>(0)), y(static_cast<T>(0)), z(static_cast<T>(0)) {}

  // FACTORY METHODS //////////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Creates an unit quaternion that represents a rotation around `axis` by `rad_angle` in radians. It is
   * assumed that `axis` has been already normalized.
   *
   * @param rad_angle
   * @param axis
   * @return constexpr Quat
   */
  constexpr static Quat rotation_axis(T rad_angle, const Vec<3, T>& axis) {
    T s = std::sin(rad_angle / static_cast<T>(2));
    return Quat{
        std::cos(rad_angle / static_cast<T>(2)),
        axis.x() * s,
        axis.y() * s,
        axis.z() * s,
    };
  }

  /**
   * @brief Creates an unit quaternion that represents a rotation around X axis by `rad_angle` in radians.
   *
   * @param rad_angle
   * @return constexpr Quat
   */
  constexpr static Quat rotation_x(T rad_angle) {
    return Quat{
        std::cos(rad_angle / static_cast<T>(2)),
        std::sin(rad_angle / static_cast<T>(2)),
        static_cast<T>(0),
        static_cast<T>(0),
    };
  }

  /**
   * @brief Creates an unit quaternion that represents a rotation around Y axis by `rad_angle` in radians.
   *
   * @param rad_angle
   * @return constexpr Quat
   */
  constexpr static Quat rotation_y(T rad_angle) {
    return Quat{
        std::cos(rad_angle / static_cast<T>(2)),
        static_cast<T>(0),
        std::sin(rad_angle / static_cast<T>(2)),
        static_cast<T>(0),
    };
  }

  /**
   * @brief Creates an unit quaternion that represents a rotation around Z axis by `rad_angle` in radians.
   *
   * @param rad_angle
   * @return constexpr Quat
   */
  constexpr static Quat rotation_z(T rad_angle) {
    return Quat{
        std::cos(rad_angle / static_cast<T>(2)),
        static_cast<T>(0),
        static_cast<T>(0),
        std::sin(rad_angle / static_cast<T>(2)),
    };
  }

  static Quat from_rotation_mat(const Mat<4, 4, T>& m) {
    // From https://github.com/g-truc/glm/blob/2d4c4b4dd31fde06cfffad7915c2b3006402322f/glm/gtc/quaternion.inl#L81
    // NOLINTBEGIN
    T fourXSquaredMinus1 = m[0][0] - m[1][1] - m[2][2];
    T fourYSquaredMinus1 = m[1][1] - m[0][0] - m[2][2];
    T fourZSquaredMinus1 = m[2][2] - m[0][0] - m[1][1];
    T fourWSquaredMinus1 = m[0][0] + m[1][1] + m[2][2];

    int biggestIndex           = 0;
    T fourBiggestSquaredMinus1 = fourWSquaredMinus1;
    if (fourXSquaredMinus1 > fourBiggestSquaredMinus1) {
      fourBiggestSquaredMinus1 = fourXSquaredMinus1;
      biggestIndex             = 1;
    }
    if (fourYSquaredMinus1 > fourBiggestSquaredMinus1) {
      fourBiggestSquaredMinus1 = fourYSquaredMinus1;
      biggestIndex             = 2;
    }
    if (fourZSquaredMinus1 > fourBiggestSquaredMinus1) {
      fourBiggestSquaredMinus1 = fourZSquaredMinus1;
      biggestIndex             = 3;
    }

    T biggestVal = sqrt(fourBiggestSquaredMinus1 + static_cast<T>(1)) * static_cast<T>(0.5);
    T mult       = static_cast<T>(0.25) / biggestVal;

    switch (biggestIndex) {
      case 0:
        return Quat{biggestVal, (m[1][2] - m[2][1]) * mult, (m[2][0] - m[0][2]) * mult, (m[0][1] - m[1][0]) * mult};
      case 1:
        return Quat{(m[1][2] - m[2][1]) * mult, biggestVal, (m[0][1] + m[1][0]) * mult, (m[2][0] + m[0][2]) * mult};
      case 2:
        return Quat{(m[2][0] - m[0][2]) * mult, (m[0][1] + m[1][0]) * mult, biggestVal, (m[1][2] + m[2][1]) * mult};
      case 3:
        return Quat{(m[0][1] - m[1][0]) * mult, (m[2][0] + m[0][2]) * mult, (m[1][2] + m[2][1]) * mult, biggestVal};
      default:  // Silence a -Wswitch-default warning in GCC. Should never actually get here. Assert is just for sanity.
        assert(false);
        return Quat{1, 0, 0, 0};
    }
    // NOLINTEND
  }

  /**
   * @brief Creates an unit quaternion that represents a rotation from the provided euler angles. The X rotation
   * is applied first.
   *
   * @param angles in radians
   * @return constexpr Quat
   */
  constexpr static Quat from_euler_xyz(const Vec<3, T>& angles) {
    return (rotation_z(angles.z()) * rotation_y(angles.y()) * rotation_x(angles.x())).normalize();
  }

  /**
   * @brief Creates a quaternion that represents a 3D point. The point is converted from homogeneous to the
   * cartesian coordinates, by dividing each of the components by the 4th component.
   *
   * @param point
   * @return constexpr Quat
   */
  constexpr static Quat point(const Vec<4, T>& point) {
    return Quat{static_cast<float>(0), point.x / point.w, point.y / point.w, point.z / point.w};
  }

  /**
   * @brief Creates a quaternion that represents a 3D point.
   *
   * @param point
   * @return constexpr Quat
   */
  constexpr static Quat point(const Vec<3, T>& point) { return Quat(point); }

  /**
   * @brief Creates a pure quaternion, i.e. a quaternion, which consists of an imaginary part only.
   *
   * @param imaginary
   * @return constexpr Quat
   */
  constexpr static Quat pure(const Vec<3, T>& imaginary) { return Quat(imaginary); }

  /**
   * @brief Creates a real quaternion, i.e. a quaternion, which consists of a real part only.
   *
   * @param imaginary
   * @return constexpr Quat
   */
  constexpr static Quat real(T real) { return Quat(real); }

  /**
   * @brief Creates a quaternion from real and imaginary part.
   *
   * @param real
   * @param imaginary
   * @return constexpr Quat
   */
  constexpr static Quat from_parts(T real, const Vec<3, T>& imaginary) { return Quat(real, imaginary); }

  /**
   * @brief Creates a quaternion consisting of zeros only.
   *
   * @return constexpr Quat
   */
  constexpr static Quat zero() { return Quat(0); }

  /**
   * @brief Creates a quaternion with the real part 1 and the imaginary part (0, 0, 0).
   *
   * @return constexpr Quat
   */
  constexpr static Quat one() { return Quat(); }

  // OPERATOR - and -= ///////////////////////////////////////////////////////////////////////////////

  [[nodiscard]] constexpr friend Quat operator-(Quat lhs, const Quat& rhs) {
    return Quat{
        lhs.w - rhs.w,
        lhs.x - rhs.x,
        lhs.y - rhs.y,
        lhs.z - rhs.z,
    };
  }

  [[nodiscard]] constexpr friend Quat operator-(Quat lhs, T rhs) {
    rhs.w -= lhs;
    return rhs;
  }

  [[nodiscard]] constexpr Quat& operator-=(T rhs) {
    w += rhs;
    return *this;
  }

  [[nodiscard]] constexpr Quat& operator-() { return *this; }

  // OPERATOR + and += ///////////////////////////////////////////////////////////////////////////////

  [[nodiscard]] constexpr friend Quat operator+(Quat lhs, const Quat& rhs) {
    return Quat{
        lhs.w + rhs.w,
        lhs.x + rhs.x,
        lhs.y + rhs.y,
        lhs.z + rhs.z,
    };
  }

  [[nodiscard]] constexpr friend Quat operator+(Quat lhs, T rhs) {
    rhs.w += lhs;
    return rhs;
  }

  [[nodiscard]] constexpr friend Quat operator+(T lhs, Quat rhs) { return rhs / lhs; }

  [[nodiscard]] constexpr Quat& operator+=(T rhs) {
    w += rhs;
    return *this;
  }

  // OPERATOR * and *= ///////////////////////////////////////////////////////////////////////////////

  [[nodiscard]] constexpr friend Vec<3, T> operator*(Quat lhs, const Vec<3, T>& rhs) {
    // TODO(migoox): make it more efficient
    return (lhs * pure(rhs) * lhs.conjugate()).imaginary();
  }

  [[nodiscard]] constexpr friend Quat operator*(Quat lhs, const Quat& rhs) {
    return Quat{
        lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,  // w
        lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,  // x
        lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,  // y
        lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w   // z
    };
  }

  [[nodiscard]] constexpr friend Quat operator*(Quat lhs, T rhs) {
    rhs.w *= lhs;
    rhs.x *= lhs;
    rhs.y *= lhs;
    rhs.z *= lhs;
    return rhs;
  }

  [[nodiscard]] constexpr friend Quat operator*(T lhs, Quat rhs) { return rhs / lhs; }

  [[nodiscard]] constexpr Quat& operator*=(T rhs) {
    w *= rhs;
    x *= rhs;
    y *= rhs;
    z *= rhs;
    return *this;
  }

  // OPERATOR / and /= ///////////////////////////////////////////////////////////////////////////////

  [[nodiscard]] constexpr friend Quat operator/(Quat lhs, T rhs) {
    lhs.w /= rhs;
    lhs.x /= rhs;
    lhs.y /= rhs;
    lhs.z /= rhs;
    return lhs;
  }

  [[nodiscard]] constexpr friend Quat operator/(T lhs, Quat rhs) { return rhs / lhs; }

  [[nodiscard]] constexpr Quat& operator/=(T rhs) {
    w /= rhs;
    x /= rhs;
    y /= rhs;
    z /= rhs;
    return *this;
  }

  // GETTERS //////////////////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Returns a real part of the quaternion.
   *
   * @return constexpr T
   */
  [[nodiscard]] constexpr T real() const { return w; }

  /**
   * @brief Returns an imaginary part of the quaternion as a 3D vector.
   *
   * @return constexpr Vec<3, T>
   */
  [[nodiscard]] constexpr Vec<3, T> imaginary() const { return Vec<3, T>{x, y, z}; }

  /**
   * @brief Computes quaternion norm.
   *
   * @return constexpr T
   */
  [[nodiscard]] constexpr T norm() const { return std::sqrt(w * w + x * x + y * y + z * z); }

  /**
   * @brief Computes conjugate of the quaternion. For unit quaternions (e.g. a rotation quaternions) it's faster
   * equivalent of inverse().
   *
   * @return constexpr Quat
   */
  [[nodiscard]] constexpr Quat conjugate() const { return Quat{w, -x, -y, -z}; }

  /**
   * @brief Computes an inverse of the quaternion. Note that if the quaternion is an unit quaternion (e.g. a rotation
   * quaternion), the conjugate() gives the same effect, but it's faster.
   *
   * @return constexpr Quat
   */
  [[nodiscard]] constexpr Quat inverse() const { return conjugate() / norm(); }

  [[nodiscard]] constexpr Quat normalize() const { return *this / norm(); }

  /**
   * @brief Returns an affine 3D rotation matrix created from unit quaternion.
   *
   * @return constexpr Mat<4, 4, T>
   */
  [[nodiscard]] constexpr Mat<4, 4, T> rot_mat() const {
    T xx = x * x;
    T yy = y * y;
    T zz = z * z;
    T xy = x * y;
    T xz = x * z;
    T yz = y * z;
    T wx = w * x;
    T wy = w * y;
    T wz = w * z;

    return Mat<4, 4, T>{Vec<4, T>{static_cast<T>(1) - static_cast<T>(2) * (yy + zz), static_cast<T>(2) * (xy + wz),
                                  static_cast<T>(2) * (xz - wy), static_cast<T>(0)},
                        Vec<4, T>{static_cast<T>(2) * (xy - wz), static_cast<T>(1) - static_cast<T>(2) * (xx + zz),
                                  static_cast<T>(2) * (yz + wx), static_cast<T>(0)},
                        Vec<4, T>{static_cast<T>(2) * (xz + wy), static_cast<T>(2) * (yz - wx),
                                  static_cast<T>(1) - static_cast<T>(2) * (xx + yy), static_cast<T>(0)},
                        Vec<4, T>{static_cast<T>(0), static_cast<T>(0), static_cast<T>(0), static_cast<T>(1)}};
  }

  /**
   * @brief Returns an 3D rotation matrix created from unit quaternion.
   *
   * @return constexpr Mat<3, 3, T>
   */
  [[nodiscard]] constexpr Mat<3, 3, T> rot_mat3() const {
    T xx = x * x;
    T yy = y * y;
    T zz = z * z;
    T xy = x * y;
    T xz = x * z;
    T yz = y * z;
    T wx = w * x;
    T wy = w * y;
    T wz = w * z;

    return Mat<3, 3, T>{Vec<3, T>{static_cast<T>(1) - static_cast<T>(2) * (yy + zz), static_cast<T>(2) * (xy + wz),
                                  static_cast<T>(2) * (xz - wy)},
                        Vec<3, T>{static_cast<T>(2) * (xy - wz), static_cast<T>(1) - static_cast<T>(2) * (xx + zz),
                                  static_cast<T>(2) * (yz + wx)},
                        Vec<3, T>{static_cast<T>(2) * (xz + wy), static_cast<T>(2) * (yz - wx),
                                  static_cast<T>(1) - static_cast<T>(2) * (xx + yy)}};
  }
};

/**
 * @brief Returns a real part of the quaternion.
 *
 * @return constexpr T
 */
template <CFloatingPoint T>
[[nodiscard]] constexpr T real(const Quat<T>& quat) {
  return quat.real();
}

/**
 * @brief Returns an imaginary part of the quaternion as a 3D vector.
 *
 * @return constexpr Vec<3, T>
 */
template <CFloatingPoint T>
[[nodiscard]] constexpr Vec<3, T> imaginary(const Quat<T>& quat) {
  return quat.imaginary();
}

/**
 * @brief Computes quaternion norm.
 *
 * @return constexpr T
 */
template <CFloatingPoint T>
[[nodiscard]] constexpr T norm(const Quat<T>& quat) {
  return quat.norm();
}

/**
 * @brief Computes conjugate of the quaternion. For unit quaternions (e.g. a rotation quaternions) it's faster
 * equivalent of inverse().
 *
 * @return constexpr Quat
 */
template <CFloatingPoint T>
[[nodiscard]] constexpr Quat<T> conjugate(const Quat<T>& quat) {
  return quat.conjugate();
}

/**
 * @brief Computes an inverse of the quaternion. Note that if the quaternion is an unit quaternion (e.g. a rotation
 * quaternion), the conjugate() gives the same effect, but it's faster.
 *
 * @return constexpr Quat
 */
template <CFloatingPoint T>
[[nodiscard]] constexpr Quat<T> inverse(const Quat<T>& unit_quat) {
  return unit_quat.inverse();
}

template <CFloatingPoint T>
[[nodiscard]] constexpr Quat<T> normalize(const Quat<T>& quat) {
  return quat.normalize();
}

/**
 * @brief Returns an affine 3D rotation matrix created from unit quaternion.
 *
 * @return constexpr Mat<4, 4, T>
 */
template <CFloatingPoint T>
[[nodiscard]] constexpr Mat<4, 4, T> rot_mat_from_quat(const Quat<T>& unit_quat) {
  return unit_quat.rot_mat();
}

/**
 * @brief Returns an 3D rotation matrix created from unit quaternion.
 *
 * @return constexpr Mat<3, 3, T>
 */
template <CFloatingPoint T>
[[nodiscard]] constexpr Mat<3, 3, T> rot_mat3_from_quat(const Quat<T>& unit_quat) {
  return unit_quat.rot_mat3();
}

template <CFloatingPoint T>
[[nodiscard]] constexpr T dot(const Quat<T>& quat1, const Quat<T>& quat2) {
  return quat1.w * quat2.w + quat1.x * quat2.x + quat1.y * quat2.y + quat1.z * quat2.z;
}

template <CFloatingPoint T>
[[nodiscard]] constexpr Quat<T> abs(const Quat<T>& quat) {
  return Quat<T>{std::abs(quat.w), std::abs(quat.x), std::abs(quat.y), std::abs(quat.z)};
}

template <CFloatingPoint T>
[[nodiscard]] constexpr bool eps_eq(const Quat<T>& quat1, const Quat<T>& quat2, const T epsilon) {
  auto q = abs(quat1 - quat2);
  return q.w < epsilon && q.x < epsilon && q.y < epsilon && q.z < epsilon;
}

template <CFloatingPoint T>
[[nodiscard]] constexpr bool eps_neq(const Quat<T>& quat1, const Quat<T>& quat2, const T epsilon) {
  auto q = abs(quat1 - quat2);
  return q.w >= epsilon && q.x >= epsilon && q.y >= epsilon && q.z >= epsilon;
}

}  // namespace eray::math

template <eray::math::CPrimitive T>
struct std::formatter<eray::math::Quat<T>> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FmtContext>
  auto format(const eray::math::Quat<T>& quat, FmtContext& ctx) const {
    return format_to(ctx.out(), "[Re={}, Im=({}, {}, {})]", quat.w, quat.x, quat.y, quat.z);
  }
};
