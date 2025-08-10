#pragma once

#include <cmath>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/types.hpp>
#include <liberay/math/vec.hpp>
#include <numbers>
#include <optional>
#include <utility>

namespace eray::math {

/**
 * @brief Represents a matrix consisting of `N` vectors, each of length `M`.
 * In a column-major interpretation, this forms a matrix with `M` rows and `N` columns.
 * In a row-major interpretation, it results in `N` rows and `M` columns.
 * Matrix multiplication can be performed in either row-major or column-major order based on preference. However
 * it's user's responsibility to assert that only one convention is used by providing a proper multiplication order.
 *
 */
template <std::size_t M, std::size_t N, CFloatingPoint T>
struct Mat {
  /**
   * @brief Default constructor that sets all cells of a matrix to 0
   *
   */
  constexpr Mat() { fill(std::make_index_sequence<N>{}, 0); }

  /**
   * @brief Constructor that sets all cells of a vector to given value by using `index_sequence`. This
   * constructor is used internally.
   *
   */
  template <std::size_t... Is>
  constexpr Mat(std::index_sequence<Is...>, T&& val) {
    ((data_[Is] = std::move(val)), ...);
  }

  /**
   * @brief Constructor that sets all vectors.
   *
   */
  template <typename... Args>
    requires(sizeof...(Args) == N) && (std::convertible_to<Args, Vec<M, T>> && ...)
  constexpr explicit Mat(Args&&... args) {
    init_data(std::make_index_sequence<N>{}, std::forward<Args>(args)...);
  }

  // FACTORY METHODS //////////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Returns a matrix with all cells set to 0
   *
   */
  constexpr static Mat zeros() { return Mat(); }

  /**
   * @brief Returns a matrix with all cells set to 1
   *
   */
  constexpr static Mat ones() { return Mat(std::make_index_sequence<N>{}, 1); }

  /**
   * @brief Returns square identity matrix
   *
   */
  constexpr static Mat identity()
    requires(N == M)
  {
    auto result = Mat();
    result.diag(std::make_index_sequence<N>{}, 1);
    return result;
  }

  /**
   * @brief Returns square matrix with diagonal values set to the provided value
   *
   */
  constexpr static Mat diag(T val)
    requires(N == M)
  {
    auto result = Mat();
    result.diag(std::make_index_sequence<N>{}, val);
    return result;
  }

  /**
   * @brief Returns a matrix with all cells set to a requested value
   *
   */
  constexpr static Mat filled(T val) { return Mat(std::make_index_sequence<N>{}, std::move(val)); }

  // OPERATOR + and += /////////////////////////////////////////////////////////////////

  friend Mat operator+(Mat lhs, const Mat& rhs) {
    add(std::make_index_sequence<N>{}, lhs.data_, rhs.data_);
    return lhs;
  }

  Mat& operator+=(const Mat& rhs) {
    add(std::make_index_sequence<N>{}, data_, rhs.data_);
    return *this;
  }

  // OPERATOR - and -= /////////////////////////////////////////////////////////////////

  friend Mat operator-(Mat lhs, const Mat& rhs) {
    sub(std::make_index_sequence<N>{}, lhs.data_, rhs.data_);
    return lhs;
  }

  Mat& operator-=(const Mat& rhs) {
    sub(std::make_index_sequence<N>{}, data_, rhs.data_);
    return *this;
  }

  // OPERATOR * and *= /////////////////////////////////////////////////////////////////

  template <std::size_t K>
  friend Mat<M, K, T> operator*(const Mat& lhs, const Mat<N, K, T>& rhs) {
    return mult(lhs, rhs);
  }

  friend Vec<M, T> operator*(Vec<M, T> lhs, const Mat& rhs) { return mult_vec_lhs(lhs, rhs); }

  friend Vec<M, T> operator*(const Mat& lhs, Vec<M, T> rhs) { return mult_vec_rhs(lhs, rhs); }

  friend Mat operator*(Mat lhs, T rhs) {
    scalar_mult(std::make_index_sequence<N>{}, lhs.data_, rhs);
    return lhs;
  }

  Mat& operator*=(T rhs) {
    scalar_mult(std::make_index_sequence<N>{}, data_, rhs);
    return *this;
  }

  Mat& operator*=(const Mat& rhs) {
    *this = mult(*this, rhs);
    return *this;
  }

  // OPERATOR [] //////////////////////////////////////////////////////////////////////////

  /**
   * @brief Returns vector with the provided index.
   *
   * @param index
   * @return T&
   */
  constexpr Vec<M, T>& operator[](std::size_t index) { return data_[index]; }

  /**
   * @brief Returns vector with the provided index.
   *
   * @param index
   * @return T&
   */
  constexpr const Vec<M, T>& operator[](std::size_t index) const { return data_[index]; }

  // GETTERS ///////////////////////////////////////////////////////////////////////////

  /**
   * @brief Returns n-th matrix element, treating the matrix as a sequence of vectors.
   *
   */
  constexpr T& nth(std::size_t index) { return data_[index / M][index % N]; }

  /**
   * @brief Returns n-th matrix element, treating the matrix as a sequence of vectors. Provided index must
   * be < than `N * M`.
   *
   */
  constexpr const T& nth(std::size_t index) const { return data_[index / M][index % N]; }

  /**
   * @brief Returns a transposition of the matrix.
   *
   */
  constexpr Mat<N, M, T> transpose() const {
    auto result = Mat<N, M, T>();
    for (int i = 0; i < M; ++i) {
      for (int j = 0; j < N; ++j) {
        result[j][i] = (*this)[i][j];
      }
    }

    return result;
  }

  // MEMORY ////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Returns a pointer to memory that stores the vectors contiguously.
   *
   * @param index
   * @return T&
   */
  T* raw_ptr() { return reinterpret_cast<T*>(data_); }

  /**
   * @brief Returns a pointer to memory that stores the vectors contiguously.
   *
   * @param index
   * @return T&
   */
  const T* raw_ptr() const { return reinterpret_cast<const T*>(data_); }

 private:
  template <typename... Args, std::size_t... Is>
  constexpr void init_data(std::index_sequence<Is...>, Args&&... args) {
    ((data_[Is] = std::forward<Args>(args)), ...);
  }

  template <std::size_t... Is>
  constexpr void fill(std::index_sequence<Is...>, T val) {
    ((data_[Is] = val), ...);
  }

  template <std::size_t... Is>
  constexpr void diag(std::index_sequence<Is...>, T val)
    requires(N == M)
  {
    ((data_[Is][Is] = val), ...);
  }

  template <CFloatingPoint... Args, std::size_t... Is>
  constexpr void diag(std::index_sequence<Is...>, Args&&... args)
    requires(N == M)
  {
    ((data_[Is][Is] = std::forward<Args>(args)), ...);
  }

  template <std::size_t... Is>
  static constexpr void add(std::index_sequence<Is...>, Vec<M, T> lhs[N], const Vec<M, T> rhs[N]) {
    ((lhs[Is] += rhs[Is]), ...);
  }

  template <std::size_t... Is>
  static constexpr void sub(std::index_sequence<Is...>, Vec<M, T> lhs[N], const Vec<M, T> rhs[N]) {
    ((lhs[Is] -= rhs[Is]), ...);
  }

  template <std::size_t K>
  static constexpr Mat<M, K, T> mult(const Mat& lhs, const Mat<N, K, T>& rhs) {
    // TODO(migoox): optimize this, especially for 4x4 matrices (maybe some sort of SIMD operation?)
    auto result = Mat<M, K, T>();
    for (std::size_t i = 0; i < M; ++i) {
      for (std::size_t j = 0; j < K; ++j) {
        for (std::size_t k = 0; k < N; ++k) {
          result[j][i] += lhs[k][i] * rhs[j][k];
        }
      }
    }
    return result;
  }

  static constexpr Vec<M, T> mult_vec_lhs(const Vec<M, T>& lhs, const Mat& rhs) {
    // TODO(migoox): optimize this (maybe some sort of SIMD operation?)
    auto result = Vec<M, T>();
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < M; ++j) {
        result[i] += lhs[j] * rhs[i][j];
      }
    }

    return result;
  }

  static constexpr Vec<M, T> mult_vec_rhs(const Mat& lhs, const Vec<M, T>& rhs) {
    // TODO(migoox): optimize this (maybe some sort of SIMD operation?)
    auto result = Vec<M, T>();
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < M; ++j) {
        result[i] += rhs[j] * lhs[j][i];
      }
    }

    return result;
  }

  template <std::size_t... Is>
  static constexpr void scalar_mult(std::index_sequence<Is...>, Vec<M, T> lhs[N], T rhs) {
    ((lhs[Is] *= rhs), ...);
  }

 private:
  Vec<M, T> data_[N];
};

/**
 * @brief Equivalent of `mat.transposed()`.
 *
 * @tparam N
 * @tparam T
 * @param vec
 * @return T
 */
template <std::size_t M, std::size_t N, CPrimitive T>
constexpr Mat<N, M, T> transpose(const Mat<M, N, T>& mat) {
  return mat.transpose();
}

/**
 * @brief Returns affine 2D scale matrix.
 *
 * @tparam Vec<2, T>
 * @param scale
 * @return Mat<3, 3, T>
 */
template <CFloatingPoint T>
Mat<3, 3, T> scale(Vec<2, T> scale) {
  return Mat<3, 3, T>{Vec<3, T>{scale.x, 0, 0}, Vec<3, T>{0, scale.y, 0}, Vec<3, T>{0, 0, 1}};
}

/**
 * @brief Returns affine 3D scale matrix.
 *
 * @tparam Vec<3, T>
 * @param scale
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> scale(Vec<3, T> scale) {
  return Mat<4, 4, T>{Vec<4, T>{scale.x, 0, 0, 0}, Vec<4, T>{0, scale.y, 0, 0}, Vec<4, T>{0, 0, scale.z, 0},
                      Vec<4, T>{0, 0, 0, 1}};
}

/**
 * @brief Returns affine 2D rotation matrix for angle in radians.
 *
 * @tparam T
 * @param rad_angle
 * @return Mat<3, 3, T>
 */
template <CFloatingPoint T>
Mat<3, 3, T> rotation(T rad_angle) {
  float c = std::cos(rad_angle);
  float s = std::sin(rad_angle);
  return Mat<3, 3, T>{Vec<3, T>{c(rad_angle), s, 0}, Vec<3, T>{-s(rad_angle), c, 0}, Vec<3, T>{0, 0, 1}};
}

/**
 * @brief Returns affine 3D rotation matrix around X axis for angle in radians.
 *
 * @tparam T
 * @param rad_angle
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> rotation_x(T rad_angle) {
  float c = std::cos(rad_angle);
  float s = std::sin(rad_angle);
  return Mat<4, 4, T>{Vec<4, T>{1, 0, 0, 0}, Vec<4, T>{0, c, s, 0}, Vec<4, T>{0, -s, c, 0}, Vec<4, T>{0, 0, 0, 1}};
}

/**
 * @brief Returns affine 3D rotation matrix around Y axis for angle in radians.
 *
 * @tparam T
 * @param rad_angle
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> rotation_y(T rad_angle) {
  float c = std::cos(rad_angle);
  float s = std::sin(rad_angle);
  return Mat<4, 4, T>{Vec<4, T>{c, 0, -s, 0}, Vec<4, T>{0, 1, 0, 0}, Vec<4, T>{s, 0, c, 0}, Vec<4, T>{0, 0, 0, 1}};
}

/**
 * @brief Returns affine 3D rotation matrix around Z axis for angle in radians.
 *
 * @tparam T
 * @param rad_angle
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> rotation_z(T rad_angle) {
  float c = std::cos(rad_angle);
  float s = std::sin(rad_angle);
  return Mat<4, 4, T>{Vec<4, T>{c, s, 0, 0}, Vec<4, T>{-s, c, 0, 0}, Vec<4, T>{0, 0, 1, 0}, Vec<4, T>{0, 0, 0, 1}};
}

/**
 * @brief Returns affine 3D rotation matrix around an arbitrary axis.
 *
 * @tparam T
 * @param rad_angle
 * @param axis (must be normalized)
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> rotation_axis(T rad_angle, Vec<3, T> axis) {
  float c = std::cos(rad_angle);
  float s = std::sin(rad_angle);
  return Mat<4, 4, T>{Vec<4, T>{axis.x * axis.x * (1 - c) + c, axis.x * axis.y * (1 - c) + axis.z * s,
                                axis.x * axis.z * (1 - c) - axis.y * s, 0},
                      Vec<4, T>{axis.x * axis.y * (1 - c) - axis.z * s, axis.y * axis.y * (1 - c) + c,
                                axis.y * axis.z * (1 - c) + axis.x * s, 0},
                      Vec<4, T>{axis.x * axis.z * (1 - c) + axis.y * s, axis.y * axis.z * (1 - c) - axis.x * s,
                                axis.z * axis.z * (1 - c) + c, 0},
                      Vec<4, T>{0, 0, 0, 1}};
}

/**
 * @brief Returns 3-dimensional 2D affine translation matrix based on the specified vector.
 *
 * @tparam T
 * @param vec
 * @return Mat<3, 3, T>
 */
template <CFloatingPoint T>
Mat<3, 3, T> translation(Vec<2, T> vec) {
  return Mat<3, 3, T>{Vec<3, T>{1, 0, 0}, Vec<3, T>{0, 1, 0}, Vec<3, T>{vec.x, vec.y, 1}};
}

/**
 * @brief Returns 4-dimensional 3D affine translation matrix based on the specified vector.
 *
 * @tparam T
 * @param vec
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> translation(Vec<3, T> vec) {
  return Mat<4, 4, T>{Vec<4, T>{1, 0, 0, 0}, Vec<4, T>{0, 1, 0, 0}, Vec<4, T>{0, 0, 1, 0},
                      Vec<4, T>{vec.x, vec.y, vec.z, 1}};
}

/**
 * @brief Right-handed perspective projection matrix with depth range -1 to 1 (OpenGL).
 *
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> frustum_gl_rh(T left, T right, T bottom, T top, T z_near, T z_far) {
  auto a = (right + left) / (right - left);
  auto b = (top + bottom) / (top - bottom);
  auto c = -(z_far + z_near) / (z_far - z_near);
  auto d = -(static_cast<T>(2) * z_far * z_near) / (z_far - z_near);
  return Mat<4, 4, T>{
      Vec<4, T>{static_cast<T>(2) * z_near / (right - left), 0, 0, 0},  //
      Vec<4, T>{0, static_cast<T>(2) * z_near / (top - bottom), 0, 0},  //
      Vec<4, T>{a, b, c, -static_cast<T>(1)},                           //
      Vec<4, T>{0, 0, d, 0}                                             //
  };
}

/**
 * @brief Right-handed perspective projection matrix with depth range -1 to 1 (OpenGL).
 *
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> perspective_gl_rh(T fovy, T aspect, T z_near, T z_far) {
  const T tan_half_fovy = std::tan(fovy / static_cast<T>(2));

  return Mat<4, 4, T>{
      Vec<4, T>{static_cast<T>(1) / (aspect * tan_half_fovy), 0, 0, 0},             //
      Vec<4, T>{0, static_cast<T>(1) / (tan_half_fovy), 0, 0},                      //
      Vec<4, T>{0, 0, -(z_far + z_near) / (z_far - z_near), -static_cast<T>(1)},    //
      Vec<4, T>{0, 0, -(static_cast<T>(2) * z_far * z_near) / (z_far - z_near), 0}  //
  };
}

/**
 * @brief Right-handed perspective projection matrix with depth range 0 to 1 (Vulkan).
 *
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> perspective_vk_rh(T fovy, T aspect, T z_near, T z_far) {
  const T tan_half_fovy = std::tan(fovy / static_cast<T>(2));

  return Mat<4, 4, T>{
      Vec<4, T>{static_cast<T>(1) / (aspect * tan_half_fovy), 0, 0, 0},  //
      Vec<4, T>{0, static_cast<T>(1) / (tan_half_fovy), 0, 0},           //
      Vec<4, T>{0, 0, z_far / (z_near - z_far), -static_cast<T>(1)},     //
      Vec<4, T>{0, 0, -(z_far * z_near) / (z_far - z_near), 0}           //
  };
}

/**
 * @brief Right-handed stereographic perspective projection matrix for right eye, with depth range -1 to 1 (OpenGL).
 *
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> stereo_right_perspective_gl_rh(T fovy, T aspect, T z_near, T z_far, T convergence) {
  const T tan_half_fovy = std::tan(fovy / static_cast<T>(2));
  T eye_separation      = convergence / static_cast<T>(30);

  return Mat<4, 4, T>{
             Vec<4, T>{static_cast<T>(1) / (aspect * tan_half_fovy), 0, 0, 0},  //
             Vec<4, T>{0, static_cast<T>(1) / (tan_half_fovy), 0, 0},           //
             Vec<4, T>{-eye_separation / static_cast<T>(2) / aspect / tan_half_fovy / convergence, 0,
                       -(z_far + z_near) / (z_far - z_near), -static_cast<T>(1)},          //
             Vec<4, T>{0, 0, -(static_cast<T>(2) * z_far * z_near) / (z_far - z_near), 0}  //
         } *
         math::translation(math::Vec3f(-eye_separation / static_cast<T>(2), 0, 0));
}

/**
 * @brief Right-handed stereographic perspective projection matrix for left eye, with depth range -1 to 1 (OpenGL).
 *
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> stereo_left_perspective_gl_rh(T fovy, T aspect, T z_near, T z_far, T convergence) {
  const T tan_half_fovy = std::tan(fovy / static_cast<T>(2));
  T eye_separation      = convergence / static_cast<T>(30);

  return Mat<4, 4, T>{
             Vec<4, T>{static_cast<T>(1) / (aspect * tan_half_fovy), 0, 0, 0},  //
             Vec<4, T>{0, static_cast<T>(1) / (tan_half_fovy), 0, 0},           //
             Vec<4, T>{eye_separation / static_cast<T>(2) / aspect / tan_half_fovy / convergence, 0,
                       -(z_far + z_near) / (z_far - z_near), -static_cast<T>(1)},          //
             Vec<4, T>{0, 0, -(static_cast<T>(2) * z_far * z_near) / (z_far - z_near), 0}  //
         } *
         math::translation(math::Vec3f(eye_separation / static_cast<T>(2), 0, 0));
}

/**
 * @brief Right-handed orthographic projection matrix with depth range -1 to 1 (OpenGL).
 *
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> orthographic_gl_rh(T left, T right, T bottom, T top, T z_near, T z_far) {
  return Mat<4, 4, T>{
      Vec<4, T>{static_cast<T>(2) / (right - left), 0, 0, 0},     //
      Vec<4, T>{0, static_cast<T>(2) / (top - bottom), 0, 0},     //
      Vec<4, T>{0, 0, -static_cast<T>(2) / (z_far - z_near), 0},  //
      Vec<4, T>{-(right + left) / (right - left), -(top + bottom) / (top - bottom),
                -(z_far + z_near) / (z_far - z_near), static_cast<T>(1)}  //
  };
}

/**
 * @brief Right-handed inverse orthographic projection matrix with depth range -1 to 1 (OpenGL).
 *
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> inv_perspective_gl_rh(T fovy, T aspect, T z_near, T z_far) {
  T const tan_half_fovy = std::tan(fovy / static_cast<T>(2));

  return Mat<4, 4, T>{
      Vec<4, T>{aspect * tan_half_fovy, 0, 0, 0},                                                   //
      Vec<4, T>{0, tan_half_fovy, 0, 0},                                                            //
      Vec<4, T>{0, 0, 0, (z_near - z_far) / (static_cast<T>(2) * z_far * z_near)},                  //
      Vec<4, T>{0, 0, -static_cast<T>(1), (z_far + z_near) / (static_cast<T>(2) * z_far * z_near)}  //
  };
}

/**
 * @brief Right-handed inverse orthographic projection matrix with depth range -1 to 1 (OpenGL).
 *
 * @return Mat<4, 4, T>
 */
template <CFloatingPoint T>
Mat<4, 4, T> inv_orthographic_gl_rh(T left, T right, T bottom, T top, T z_near, T z_far) {
  // TODO(migoox): check correctness
  return Mat<4, 4, T>{
      Vec<4, T>{(right - left) / static_cast<T>(2), 0, 0, 0},     //
      Vec<4, T>{0, (top - bottom) / static_cast<T>(2), 0, 0},     //
      Vec<4, T>{0, 0, (z_far - z_near) / static_cast<T>(-2), 0},  //
      Vec<4, T>{(right + left) / static_cast<T>(2), (top + bottom) / static_cast<T>(2),
                (z_far + z_near) / static_cast<T>(2), static_cast<T>(1)}  //
  };
}

template <CFloatingPoint T>
constexpr std::optional<Mat<4, 4, T>> inverse(const Mat<4, 4, T>& m) {
  const T coef00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
  const T coef02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
  const T coef03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];
  const T coef04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
  const T coef06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
  const T coef07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];
  const T coef08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
  const T coef10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
  const T coef11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];
  const T coef12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
  const T coef14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
  const T coef15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];
  const T coef16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
  const T coef18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
  const T coef19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];
  const T coef20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
  const T coef22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
  const T coef23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

  auto fac0 = Vec<4, T>{coef00, coef00, coef02, coef03};
  auto fac1 = Vec<4, T>{coef04, coef04, coef06, coef07};
  auto fac2 = Vec<4, T>{coef08, coef08, coef10, coef11};
  auto fac3 = Vec<4, T>{coef12, coef12, coef14, coef15};
  auto fac4 = Vec<4, T>{coef16, coef16, coef18, coef19};
  auto fac5 = Vec<4, T>{coef20, coef20, coef22, coef23};

  auto vec0 = Vec<4, T>{m[1][0], m[0][0], m[0][0], m[0][0]};
  auto vec1 = Vec<4, T>{m[1][1], m[0][1], m[0][1], m[0][1]};
  auto vec2 = Vec<4, T>{m[1][2], m[0][2], m[0][2], m[0][2]};
  auto vec3 = Vec<4, T>{m[1][3], m[0][3], m[0][3], m[0][3]};

  auto inv0 = vec1 * fac0 - vec2 * fac1 + vec3 * fac2;
  auto inv1 = vec0 * fac0 - vec2 * fac3 + vec3 * fac4;
  auto inv2 = vec0 * fac1 - vec1 * fac3 + vec3 * fac5;
  auto inv3 = vec0 * fac2 - vec1 * fac4 + vec2 * fac5;

  auto sign_a = Vec<4, T>{+static_cast<T>(1), -static_cast<T>(1), +static_cast<T>(1), -static_cast<T>(1)};
  auto sign_b = Vec<4, T>{-static_cast<T>(1), +static_cast<T>(1), -static_cast<T>(1), +static_cast<T>(1)};

  auto inverse = Mat<4, 4, T>::identity();
  inverse[0]   = inv0 * sign_a;
  inverse[1]   = inv1 * sign_b;
  inverse[2]   = inv2 * sign_a;
  inverse[3]   = inv3 * sign_b;

  auto row0 = Vec<4, T>{inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0]};
  auto col0 = m[0];
  auto dot0 = col0 * row0;
  auto det  = dot0[0] + dot0[1] + dot0[2] + dot0[3];

  if (std::abs(det) < static_cast<T>(0.000001F)) {
    return std::nullopt;
  }

  return inverse * (static_cast<T>(1) / det);
}

namespace internal {

template <typename T>
constexpr bool is_zero(T value) {
  return std::abs(value) < std::numeric_limits<T>::epsilon();
}

}  // namespace internal

/**
 * @brief Extracts the euler angles from the given rotation matrix assuming XYZ order. For column-major it's Z * Y * X.
 * For row-major it's X * Y * Z.
 *
 * @tparam T
 * @param mat
 * @return Vec<3, T>
 */
template <CFloatingPoint T, std::size_t N>
[[nodiscard]] Vec<3, T> eulers_xyz(const Mat<N, N, T>& mat)
  requires(N == 3 || N == 4)
{
  auto eulers = Vec<3, T>();
  if (internal::is_zero(static_cast<T>(1) - std::abs(eulers.y))) {
    eulers.z = static_cast<T>(0);
    if (internal::is_zero(mat[0][2] + static_cast<T>(1))) {
      eulers.y = std::numbers::pi / static_cast<T>(2);
      eulers.x = std::atan2(mat[1][0], mat[0][2]);
    } else {
      eulers.y = -std::numbers::pi / static_cast<T>(2);
      eulers.x = std::atan2(-mat[1][0], -mat[0][2]);
    }
  } else {
    auto k   = std::cos(eulers.y);
    eulers.y = -std::asin(mat[0][2]);
    eulers.x = std::atan2(mat[1][2] / k, mat[2][2] / k);
    eulers.z = std::atan2(mat[0][1] / k, mat[0][0] / k);
  }
  return eulers;
}

}  // namespace eray::math

template <std::size_t M, std::size_t N, eray::math::CFloatingPoint T>
struct std::formatter<eray::math::Mat<M, N, T>> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FmtContext>
  auto format(const eray::math::Mat<M, N, T>& mat, FmtContext& ctx) const {
    auto out = format_to(ctx.out(), "[");

    for (size_t i = 0; i < N; ++i) {
      out = format_to(out, "{}", mat[i]);

      if (i < N - 1) {
        out = format_to(out, ", ");
      }
    }

    return format_to(out, "]");
  }
};
