#include <liberay/math/types.hpp>

namespace eray::math {

template <CFloatingPoint T>
struct Quat;

using Quatf = Quat<float>;
using Quatd = Quat<double>;

}  // namespace eray::math
