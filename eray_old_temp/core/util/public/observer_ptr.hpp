#pragma once

#include <liberay/util/ruleof.hpp>

namespace eray::util {

template <typename WatchedObject>
class ObserverPtr {
 public:
  explicit ObserverPtr(WatchedObject& obj) : ptr_(&obj) {}

  ERAY_DELETE_COPY(ObserverPtr)
  ERAY_DEFAULT_MOVE(ObserverPtr)

  WatchedObject* operator->() { return ptr_; }
  WatchedObject& operator*() { return *ptr_; }
  const WatchedObject* operator->() const { return ptr_; }
  const WatchedObject& operator*() const { return *ptr_; }

  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  WatchedObject* ptr_;
};

}  // namespace eray::util
