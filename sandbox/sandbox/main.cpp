#include <liberay/math/quat.hpp>
#include <liberay/res/image.hpp>
#include <print>

int main() {
  auto t  = eray::res::Image(50, 50);
  auto q1 = eray::math::Quatf::one();
  auto q2 = eray::math::Quatf::rotation_y(eray::math::radians(30.F));
  auto q  = q1 * q2;
  std::print("{}", q);
  return 0;
}
