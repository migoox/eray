#pragma once

namespace eray::util {

// from: https://www.cppstories.com/2018/06/variant/#overload

template <class... Ts>
struct match : Ts... {  // NOLINT
  using Ts::operator()...;
};

template <class... Ts>
match(Ts...) -> match<Ts...>;

}  // namespace eray::util
