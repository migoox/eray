#pragma once

#include <cstdint>
#include <liberay/util/hash_combine.hpp>
#include <liberay/util/ruleof.hpp>
#include <type_traits>

namespace eray::util {

struct Any {};

template <typename TObject>
struct Handle {
  using Object   = TObject;
  using ObjectId = std::uint32_t;

  std::uint32_t owner_signature;
  std::uint32_t timestamp;
  std::uint32_t obj_id;

  Handle() = delete;
  Handle(std::uint32_t _owner_signature, std::uint32_t _timestamp, std::uint32_t _obj_id)
      : owner_signature(_owner_signature), timestamp(_timestamp), obj_id(_obj_id) {}

  template <typename OtherObject>
  explicit Handle(const Handle<OtherObject>& other)
    requires(std::is_same_v<Object, util::Any>)
      : owner_signature(other.owner_signature), timestamp(other.timestamp), obj_id(other.obj_id) {}
};

template <typename T>
bool operator==(const Handle<T>& lhs, const Handle<T>& rhs) {
  return lhs.owner_signature == rhs.owner_signature && lhs.timestamp == rhs.timestamp && lhs.obj_id == rhs.obj_id;
}

template <typename T>
bool operator!=(const Handle<T>& lhs, const Handle<T>& rhs) {
  return lhs != rhs;
}

using AnyObjectHandle = Handle<Any>;

}  // namespace eray::util

namespace std {
template <typename Object>
struct hash<eray::util::Handle<Object>> {
  size_t operator()(const eray::util::Handle<Object>& handle) const noexcept {
    size_t h1 = hash<uint32_t>{}(handle.owner_signature);
    size_t h2 = hash<uint32_t>{}(handle.timestamp);
    size_t h3 = hash<uint32_t>{}(handle.obj_id);
    eray::util::hash_combine(h1, h2);
    eray::util::hash_combine(h1, h3);

    return h1;
  }
};
}  // namespace std
