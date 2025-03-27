#pragma once

namespace eray::util {

// from: https://www.cppstories.com/2018/06/variant/#overload

template <class... Ts>
struct Overload : Ts... {
  using Ts::operator()...;
};

template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

}  // namespace eray::util
