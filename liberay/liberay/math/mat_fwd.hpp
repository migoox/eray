#pragma once
#include <liberay/math/types.hpp>

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
struct Mat;

using Mat2f = Mat<2, 2, float>;
using Mat2d = Mat<2, 2, double>;

using Mat3f = Mat<3, 3, float>;
using Mat3d = Mat<3, 3, double>;

using Mat4f = Mat<4, 4, float>;
using Mat4d = Mat<4, 4, double>;

}  // namespace eray::math
