#pragma once

#include <cmath>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/types.hpp>
#include <liberay/math/vec.hpp>
#include <numbers>
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
    requires(sizeof...(Args) == N) && (std::convertible_to<Args, Vec<N, T>> && ...)
  explicit Mat(Args&&... args) {
    init_data(std::make_index_sequence<N>{}, std::forward<Args>(args)...);
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                              FACTORY METHODS                                     //
  //////////////////////////////////////////////////////////////////////////////////////

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

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR + and +=                                    //
  //////////////////////////////////////////////////////////////////////////////////////

  friend Mat operator+(Mat lhs, const Mat& rhs) {
    add(std::make_index_sequence<N>{}, lhs.data_, rhs.data_);
    return lhs;
  }

  Mat& operator+=(const Mat& rhs) {
    add(std::make_index_sequence<N>{}, data_, rhs.data_);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR - and -=                                    //
  //////////////////////////////////////////////////////////////////////////////////////

  friend Mat operator-(Mat lhs, const Mat& rhs) {
    sub(std::make_index_sequence<N>{}, lhs.data_, rhs.data_);
    return lhs;
  }

  Mat& operator-=(const Mat& rhs) {
    sub(std::make_index_sequence<N>{}, data_, rhs.data_);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR * and *=                                    //
  //////////////////////////////////////////////////////////////////////////////////////

  template <std::size_t K>
  friend Mat<M, K, T> operator*(const Mat& lhs, const Mat<N, K, T>& rhs) {
    return mult(lhs, rhs);
  }

  friend Vec<M, T> operator*(Vec<M, T> lhs, const Mat& rhs) { return mult_vec_lhs(lhs, rhs); }

  friend Vec<M, T> operator*(const Mat& lhs, Vec<M, T> rhs) { return mult_vec_rhs(lhs, rhs); }

  friend Mat operator*(Mat lhs, T rhs) {
    scalar_mult(lhs, rhs);
    return lhs;
  }

  friend Mat operator*(T rhs, const Mat& lhs) { return rhs * lhs; }

  Mat& operator*=(T rhs) {
    scalar_mult(std::make_index_sequence<N>{}, data_, rhs);
    return *this;
  }

  Mat& operator*=(const Mat& rhs) {
    *this = mult(*this, rhs);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR []                                          //
  //////////////////////////////////////////////////////////////////////////////////////

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

  //////////////////////////////////////////////////////////////////////////////////////
  //                                 GETTERS                                          //
  //////////////////////////////////////////////////////////////////////////////////////

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
  constexpr Mat<N, M, T> transposed() const {
    auto result = Mat<N, M, T>();
    for (int i = 0; i < M; ++i) {
      for (int j = 0; j < N; ++j) {
        result[j][i] = (*this)[i][j];
      }
    }

    return result;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                                    MEMORY                                        //
  //////////////////////////////////////////////////////////////////////////////////////

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
  const T* raw_ptr() const { return reinterpret_cast<T*>(data_); }

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
    for (int i = 0; i < M; ++i) {
      for (int j = 0; j < K; ++j) {
        for (int k = 0; k < N; ++k) {
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
  static constexpr void scalar_mult(std::index_sequence<Is...>, Vec<M, T> lhs[N], const Vec<M, T> rhs[N]) {
    ((lhs[Is] *= rhs[Is]), ...);
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
  return mat.transposed();
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
[[nodiscard]] Vec<3, T> eulers_xyz(Mat<N, N, T> mat)
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
