#pragma once

#include <expected>
#include <liberay/util/logger.hpp>

namespace eray::util {

template <class T, class E>
struct Result : public std::expected<T, E> {
  using base = std::expected<T, E>;
  using base::base;  // inherit constructors

  T or_panic(const std::source_location& l = std::source_location::current()) {
    if (this->has_value()) {
      return std::move(this->value());
    }
    Logger::instance().log(LogLevel::Err, false, l, "Program has crashed!");
    std::abort();
  }
};

template <class E>
struct Result<void, E> : public std::expected<void, E> {
  using base = std::expected<void, E>;
  using base::base;  // inherit constructors

  void or_panic(const std::source_location& l = std::source_location::current()) {
    if (this->has_value()) {
      return;
    }
    Logger::instance().log(LogLevel::Err, false, l, "Program has crashed!");
    std::abort();
  }
};

}  // namespace eray::util
