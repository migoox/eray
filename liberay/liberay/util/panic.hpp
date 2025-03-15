#pragma once
#include <liberay/util/logger.hpp>

namespace eray::util {

template <typename... Args>
inline static void panic(Logger::FormatWithLocation fmt_loc, const Args&... args) {
  Logger::instance().log(LogLevel::Err, false, fmt_loc.loc, fmt_loc.value, args...);
  Logger::err("Program has crashed!");
  std::abort();
}

inline static void not_impl_yet(const std::source_location& l = std::source_location::current()) {
  Logger::instance().log(LogLevel::Err, false, l, "Not implemented yet.");
  Logger::err("Program has crashed!");
  std::abort();
}

}  // namespace eray::util
