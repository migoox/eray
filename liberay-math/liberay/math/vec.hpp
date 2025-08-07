#pragma once
#include <cmath>
#include <format>
#include <initializer_list>
#include <liberay/math/types.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <numbers>
#include <utility>

#ifdef _MSC_VER
#define ERAY_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define ERAY_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace eray::math {

namespace internal {

struct Empty {};

}  // namespace internal

template <std::size_t N, CPrimitive T>
  requires(N >= 1)
struct Vec {
  union {
    T data[N];
    struct {
      ERAY_NO_UNIQUE_ADDRESS std::conditional_t<(N >= 1), T, internal::Empty> x;
      ERAY_NO_UNIQUE_ADDRESS std::conditional_t<(N >= 2), T, internal::Empty> y;
      ERAY_NO_UNIQUE_ADDRESS std::conditional_t<(N >= 3), T, internal::Empty> z;
      ERAY_NO_UNIQUE_ADDRESS std::conditional_t<(N >= 4), T, internal::Empty> w;
    };
    struct {
      ERAY_NO_UNIQUE_ADDRESS std::conditional_t<(N >= 3 && N <= 4), T, internal::Empty> r;
      ERAY_NO_UNIQUE_ADDRESS std::conditional_t<(N >= 3 && N <= 4), T, internal::Empty> g;
      ERAY_NO_UNIQUE_ADDRESS std::conditional_t<(N >= 3 && N <= 4), T, internal::Empty> b;
      ERAY_NO_UNIQUE_ADDRESS std::conditional_t<(N == 4), T, internal::Empty> a;
    };
  };

  /**
   * @brief Default constructor that sets all components of a vector to 0
   *
   */
  constexpr Vec() { fill(std::make_index_sequence<N>{}, 0); }

  /**
   * @brief Constructor that sets all components of a vector
   *
   */
  template <typename... Args>
    requires(sizeof...(Args) == N) && (std::convertible_to<Args, T> && ...)
  explicit constexpr Vec(Args&&... args) {
    init_data(std::make_index_sequence<N>{}, std::forward<Args>(args)...);
  }

  /**
   * @brief Constructor that sets all components of a vector
   *
   */
  template <std::size_t N2, typename... Args>
    requires(sizeof...(Args) + N2 == N) && (std::convertible_to<Args, T> && ...)
  explicit constexpr Vec(const Vec<N2, T>& other, Args&&... args) {
    init_data(std::make_index_sequence<N2>{}, other.data);
    init_data<N2>(std::make_index_sequence<N - N2>{}, std::forward<Args>(args)...);
  }

  /**
   * @brief Constructor that sets all components of a vector
   *
   */
  template <typename T2>
    requires(std::convertible_to<T2, T>)
  explicit constexpr Vec(const Vec<N, T2>& other) {
    init_data(std::make_index_sequence<N>{}, other.data);
  }

  /**
   * @brief Constructor that sets all components of a vector to given value by using `index_sequence`. This
   * constructor is used internally.
   *
   */
  template <typename... Args, std::size_t... Is>
  constexpr Vec(std::index_sequence<Is...>, T&& val) {
    ((data[Is] = std::move(val)), ...);
  }

  /**
   * @brief Constructor for changing dimensionality of a vector.
   *
   * @tparam K
   */
  template <std::size_t K>
  explicit constexpr Vec(const Vec<K, T>& vec)
    requires(K > N)
  {
    fill(std::make_index_sequence<N>(), vec);
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                              FACTORY METHODS                                     //
  //////////////////////////////////////////////////////////////////////////////////////

  /**
   * @brief Sets all components of a vector to 0
   *
   */
  constexpr static Vec zeros() { return Vec(); }

  /**
   * @brief Sets all components of a vector to 1
   *
   */
  constexpr static Vec ones() { return Vec(std::make_index_sequence<N>{}, 1); }

  /**
   * @brief Sets all components of a vector to requested value
   *
   */
  constexpr static Vec filled(T val) { return Vec(std::make_index_sequence<N>{}, std::move(val)); }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR =                                           //
  //////////////////////////////////////////////////////////////////////////////////////

  constexpr Vec& operator=(T val) {
    fill(std::make_index_sequence<N>{}, val);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR + and +=                                    //
  //////////////////////////////////////////////////////////////////////////////////////

  friend Vec operator+(Vec lhs, const Vec& rhs) {
    add(std::make_index_sequence<N>{}, lhs.data, rhs.data);
    return lhs;
  }

  friend Vec operator+(Vec lhs, T rhs) {
    lhs += rhs;
    return lhs;
  }

  friend Vec operator+(T lhs, const Vec& rhs) { return rhs + lhs; }

  Vec& operator+=(const Vec& rhs) {
    add(std::make_index_sequence<N>{}, data, rhs.data);
    return *this;
  }

  Vec& operator+=(T rhs) {
    scalar_add(std::make_index_sequence<N>{}, data, rhs);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR - and -=                                    //
  //////////////////////////////////////////////////////////////////////////////////////

  Vec operator-() const { return *this * (static_cast<T>(-1)); }

  friend Vec operator-(Vec lhs, T rhs) {
    lhs -= rhs;
    return lhs;
  }

  friend Vec operator-(T lhs, const Vec& rhs) { return rhs - lhs; }

  friend Vec operator-(Vec lhs, const Vec& rhs) {
    sub(std::make_index_sequence<N>{}, lhs.data, rhs.data);
    return lhs;
  }

  Vec& operator-=(const Vec& rhs) {
    sub(std::make_index_sequence<N>{}, data, rhs.data);
    return *this;
  }

  Vec& operator-=(T rhs) {
    scalar_add(std::make_index_sequence<N>{}, data, -rhs);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR * and *=                                    //
  //////////////////////////////////////////////////////////////////////////////////////

  friend Vec operator*(Vec lhs, const Vec& rhs) {
    mult(std::make_index_sequence<N>{}, lhs.data, rhs.data);
    return lhs;
  }

  friend Vec operator*(Vec lhs, T rhs) {
    lhs *= rhs;
    return lhs;
  }

  friend Vec operator*(T lhs, const Vec& rhs) { return rhs * lhs; }

  Vec& operator*=(T rhs) {
    scalar_mult(std::make_index_sequence<N>{}, data, rhs);
    return *this;
  }

  Vec& operator*=(const Vec& rhs) {
    scalar_mult(std::make_index_sequence<N>{}, data, rhs);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR / and /=                                    //
  //////////////////////////////////////////////////////////////////////////////////////

  friend Vec operator/(Vec lhs, const Vec& rhs) {
    div(std::make_index_sequence<N>{}, lhs.data, rhs.data);
    return lhs;
  }

  friend Vec operator/(T lhs, Vec rhs) {
    scalar_div(std::make_index_sequence<N>{}, lhs, rhs.data);
    return rhs;
  }

  friend Vec operator/(Vec lhs, T rhs) {
    lhs /= rhs;
    return lhs;
  }

  Vec& operator/=(T rhs) {
    scalar_div(std::make_index_sequence<N>{}, data, rhs);
    return *this;
  }

  Vec& operator/=(const Vec& rhs) {
    scalar_div(std::make_index_sequence<N>{}, data, rhs);
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                             OPERATOR []                                          //
  //////////////////////////////////////////////////////////////////////////////////////

  T& operator[](std::size_t index) { return data[index]; }

  const T& operator[](std::size_t index) const { return data[index]; }

  //////////////////////////////////////////////////////////////////////////////////////
  //                                       GETTERS                                    //
  //////////////////////////////////////////////////////////////////////////////////////

  auto length() const { return length(std::make_index_sequence<N>{}, data); }

  Vec normalize() const { return *this / this->length(); }

  Vec abs() const {
    auto v = Vec(*this);
    abs(std::make_index_sequence<N>{}, v.data);
    return v;
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //                                    MEMORY                                        //
  //////////////////////////////////////////////////////////////////////////////////////
  /**
   * @brief Returns a pointer to memory that stores the vector components contiguously.
   *
   * @param index
   * @return T&
   */
  T* raw_ptr() { return reinterpret_cast<T*>(data); }

  /**
   * @brief Returns a pointer to memory that stores the vector components contiguously.
   *
   * @param index
   * @return T&
   */
  const T* raw_ptr() const { return reinterpret_cast<T*>(data); }

 private:
  template <typename... Args, std::size_t... Is>
  constexpr void init_data(std::index_sequence<Is...>, Args&&... args) {
    ((data[Is] = std::forward<Args>(args)), ...);
  }

  template <std::size_t Start, typename... Args, std::size_t... Is>
  constexpr void init_data(std::index_sequence<Is...>, Args&&... args) {
    ((data[Start + Is] = std::forward<Args>(args)), ...);
  }

  template <typename T2, std::size_t... Is>
    requires(std::convertible_to<T2, T>)
  constexpr void init_data(std::index_sequence<Is...>, const T2* vec_data) {
    ((data[Is] = static_cast<T>(vec_data[Is])), ...);
  }

  template <std::size_t Start, typename T2, std::size_t... Is>
    requires(std::convertible_to<T2, T>)
  constexpr void init_data(std::index_sequence<Is...>, const T2* vec_data) {
    ((data[Start + Is] = static_cast<T>(vec_data[Is])), ...);
  }

  template <std::size_t... Is>
  constexpr void fill(std::index_sequence<Is...>, T val) {
    ((data[Is] = val), ...);
  }

  template <std::size_t... Is, std::size_t K>
  constexpr void fill(std::index_sequence<Is...>, const Vec<K, T>& vec)
    requires(K >= N)
  {
    ((data[Is] = vec.data[Is]), ...);
  }

  template <std::size_t... Is>
  static constexpr void add(std::index_sequence<Is...>, T lhs[N], const T rhs[N]) {
    ((lhs[Is] += rhs[Is]), ...);
  }

  template <std::size_t... Is>
  static constexpr void scalar_add(std::index_sequence<Is...>, T data[N], T scalar) {
    ((data[Is] += scalar), ...);
  }

  template <std::size_t... Is>
  static constexpr void sub(std::index_sequence<Is...>, T lhs[N], const T rhs[N]) {
    ((lhs[Is] -= rhs[Is]), ...);
  }

  template <std::size_t... Is>
  static constexpr void mult(std::index_sequence<Is...>, T lhs[N], const T rhs[N]) {
    ((lhs[Is] *= rhs[Is]), ...);
  }

  template <std::size_t... Is>
  static constexpr void scalar_mult(std::index_sequence<Is...>, T data[N], T scalar) {
    ((data[Is] *= scalar), ...);
  }

  template <std::size_t... Is>
  static constexpr void div(std::index_sequence<Is...>, T lhs[N], const T rhs[N]) {
    ((lhs[Is] /= rhs[Is]), ...);
  }

  template <std::size_t... Is>
  static constexpr void inv_sign(std::index_sequence<Is...>, T data[N]) {
    ((data[Is] = -data[Is]), ...);
  }

  template <std::size_t... Is>
  static constexpr void scalar_div(std::index_sequence<Is...>, T data[N], T scalar) {
    ((data[Is] /= scalar), ...);
  }

  template <std::size_t... Is>
  static constexpr void scalar_div(std::index_sequence<Is...>, T scalar, T data[N]) {
    ((data[Is] = scalar / data[Is]), ...);
  }

  template <std::size_t... Is>
  static constexpr auto length(std::index_sequence<Is...>, const T data[N]) {
    T val = 0;
    ((val += data[Is] * data[Is]), ...);
    return std::sqrt(val);
  }

  template <std::size_t... Is>
  static constexpr auto abs(std::index_sequence<Is...>, T data[N]) {
    ((data[Is] = std::abs(data[Is])), ...);
  }
};

namespace internal {

template <std::size_t... Is, CPrimitive T>
constexpr T dot_base(std::index_sequence<Is...>, const T* lhs, const T* rhs) {
  T val = 0;
  ((val += lhs[Is] * rhs[Is]), ...);
  return val;
}

template <std::size_t... Is, CPrimitive T>
constexpr void clamp_base(std::index_sequence<Is...>, T* data, T min, T max) {
  ((data[Is] = std::clamp(data[Is], min, max)), ...);  // C++17 std::clamp
}

template <std::size_t... Is, CPrimitive T>
constexpr void clamp_base(std::index_sequence<Is...>, T* data, const T* min_data, const T* max_data) {
  ((data[Is] = std::clamp(data[Is], min_data[Is], max_data[Is])), ...);
}

template <std::size_t... Is, CPrimitive T>
constexpr void min_base(std::index_sequence<Is...>, T* data, const T* vec1_data, const T* vec2_data) {
  ((data[Is] = std::min(vec1_data[Is], vec2_data[Is])), ...);
}
template <std::size_t... Is, CPrimitive T>
constexpr void max_base(std::index_sequence<Is...>, T* data, const T* vec1_data, const T* vec2_data) {
  ((data[Is] = std::max(vec1_data[Is], vec2_data[Is])), ...);
}

template <std::size_t... Is, CPrimitive T>
constexpr bool all_components_less_than(std::index_sequence<Is...>, T* data, const T value) {
  bool result = true;
  ((data[Is] = result && (data[Is] < value)), ...);
  return result;
}

}  // namespace internal

/**
 * @brief Performs a cross product on 3-dimensional vectors.
 *
 * @tparam Vec<N, T>
 * @param lhs
 * @param rhs
 * @return T
 */
template <std::size_t N, CPrimitive T>
constexpr T dot(const Vec<N, T>& lhs, const Vec<N, T>& rhs) {
  return internal::dot_base(std::make_index_sequence<N>(), lhs.data, rhs.data);
}

/**
 * @brief Computes the z-component of the 3D cross product between two 2D
 * vectors. Treats `lhs` and `rhs` as 3-dimensional vectors with their
 * z-components set to 0. Useful for determining the perpendicular relationship
 * between two 2D vectors.
 *
 * @tparam Vec<2, T>
 * @param lhs
 * @param rhs
 * @return T
 */
template <CPrimitive T>
constexpr T cross(const Vec<2, T>& lhs, const Vec<2, T>& rhs) {
  return lhs[0] * rhs[1] - lhs[1] * rhs[0];
}

/**
 * @brief Performs a cross product on 3-dimensional vectors.
 *
 * @tparam Vec<3, T>
 * @param lhs
 * @param rhs
 * @return Vec<3, T>
 */
template <CPrimitive T>
constexpr Vec<3, T> cross(const Vec<3, T>& lhs, const Vec<3, T>& rhs) {
  return Vec<3, T>(lhs[1] * rhs[2] - lhs[2] * rhs[1], lhs[2] * rhs[0] - lhs[0] * rhs[2],
                   lhs[0] * rhs[1] - lhs[1] * rhs[0]);
}

/**
 * @brief Returns a normalized vector. Equivalent of `vec.normalized()`.
 *
 * @tparam N
 * @tparam T
 * @param vec
 * @return Vec<N, T>
 */
template <std::size_t N, CPrimitive T>
constexpr Vec<N, T> normalize(const Vec<N, T>& vec) {
  return vec.normalize();
}

/**
 * @brief Equivalent of `vec.length()`.
 *
 * @tparam N
 * @tparam T
 * @param vec
 * @return T
 */
template <std::size_t N, CPrimitive T>
constexpr auto length(const Vec<N, T>& vec) {
  return vec.length();
}

/**
 * @brief Equivalent of `vec.abs()`.
 *
 * @tparam N
 * @tparam T
 * @param vec
 * @return constexpr auto
 */
template <std::size_t N, CPrimitive T>
constexpr auto abs(const Vec<N, T>& vec) {
  return vec.abs();
}

/**
 * @brief Returns a distance between two vectors.
 *
 * @tparam N
 * @tparam T
 * @param vec
 * @return T
 */
template <std::size_t N, CPrimitive T>
constexpr T distance(const Vec<N, T>& lhs, const Vec<N, T>& rhs) {
  T val = 0;
  for (std::size_t i = 0; i < N; ++i) {
    val += (rhs[i] - lhs[i]) * (rhs[i] - lhs[i]);
  }
  return std::sqrt(val);
}

template <CFloatingPoint T>
[[nodiscard]] T radians(T deg) {
  return deg / static_cast<T>(180) * std::numbers::pi_v<T>;
}

template <CFloatingPoint T, std::size_t N>
[[nodiscard]] Vec<N, T> radians(const Vec<N, T>& deg) {
  return deg / static_cast<T>(180) * std::numbers::pi_v<T>;
}

template <CFloatingPoint T>
[[nodiscard]] T degrees(T radians) {
  return radians * static_cast<T>(180) / std::numbers::pi_v<T>;
}

template <CFloatingPoint T, std::size_t N>
[[nodiscard]] Vec<N, T> degrees(const Vec<N, T>& radians) {
  return radians * static_cast<T>(180) / std::numbers::pi_v<T>;
}

template <CFloatingPoint T, std::size_t N>
[[nodiscard]] Vec<N, T> clamp(const Vec<N, T>& vec, T min, T max) {
  auto result = vec;
  internal::clamp_base(std::make_index_sequence<N>(), result.data, min, max);
  return result;
}

template <CFloatingPoint T, std::size_t N>
[[nodiscard]] Vec<N, T> clamp(const Vec<N, T>& vec, const Vec<N, T>& min, const Vec<N, T>& max) {
  auto result = vec;
  internal::clamp_base(std::make_index_sequence<N>(), result.data, min.data, max.data);
  return result;
}

template <CFloatingPoint T, std::size_t N>
[[nodiscard]] Vec<N, T> min(const Vec<N, T>& vec1, const Vec<N, T>& vec2) {
  auto result = Vec<N, T>::filled(0.F);
  internal::min_base(std::make_index_sequence<N>(), result.data, vec1.data, vec2.data);
  return result;
}

template <CFloatingPoint T, std::size_t N>
[[nodiscard]] Vec<N, T> max(const Vec<N, T>& vec1, const Vec<N, T>& vec2) {
  auto result = Vec<N, T>::filled(0.F);
  internal::max_base(std::make_index_sequence<N>(), result.data, vec1.data, vec2.data);
  return result;
}

template <CFloatingPoint T, std::size_t N>
[[nodiscard]] bool eps_eq(const Vec<N, T>& vec1, const Vec<N, T>& vec2, const T epsilon) {
  auto t = eray::math::abs(vec1 - vec2);
  return internal::all_components_less_than(std::make_index_sequence<N>(), t.data, epsilon);
}

template <CFloatingPoint T, std::size_t N>
[[nodiscard]] bool eps_neq(const Vec<N, T>& vec1, const Vec<N, T>& vec2, const T epsilon) {
  auto t = eray::math::abs(vec1 - vec2);
  return !internal::all_components_less_than(std::make_index_sequence<N>(), t.data, epsilon);
}

}  // namespace eray::math

template <std::size_t N, eray::math::CPrimitive T>
struct std::formatter<eray::math::Vec<N, T>> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FmtContext>
  auto format(const eray::math::Vec<N, T>& vec, FmtContext& ctx) const {
    auto out = format_to(ctx.out(), "[");

    for (size_t i = 0; i < N; ++i) {
      out = format_to(out, "{}", vec.data[i]);

      if (i < N - 1) {
        out = format_to(out, ", ");
      }
    }

    return format_to(out, "]");
  }
};
