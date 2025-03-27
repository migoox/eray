#pragma once
#include <functional>

namespace eray::util {

template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
  // http://stackoverflow.com/questions/7222143/unordered-map-hash-function-c
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

}  // namespace eray::util
