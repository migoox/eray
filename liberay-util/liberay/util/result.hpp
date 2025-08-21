#pragma once

#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/zstring_view.hpp>
#include <source_location>

namespace eray::util {

struct ResultFmtWithLoc {
  const char* value;
  std::source_location loc;

  // NOLINTBEGIN
  ResultFmtWithLoc(const char* s = "", const std::source_location& l = std::source_location::current())
      : value(s), loc(l) {}
  // NOLINTEND
};

template <typename TLogger, typename TError>
concept CResultLogger = requires(std::source_location loc, const TError& err, zstring_view msg) {
  TLogger::log_panic(loc, err);
  TLogger::log_panic(loc, err, msg);
};

template <typename TType, typename TError, CResultLogger<TError> TResultLogger>
struct ResultBase : public std::expected<TType, TError> {
  using base = std::expected<TType, TError>;
  using base::base;  // inherit constructors

  ResultBase(const std::expected<TType, TError>& exp) : base(exp) {}        // NOLINT
  ResultBase(std::expected<TType, TError>&& exp) : base(std::move(exp)) {}  // NOLINT

  /**
   * @brief In the case the error is returned, program crashes. Inspired by
   *
   * @param fmt_loc
   * @return TType
   */
  TType or_panic(ResultFmtWithLoc fmt_loc = {}) {
    if (this->has_value()) {
      return std::move(this->value());
    }
    TResultLogger::log_panic(fmt_loc.loc, this->error(), fmt_loc.value);
    std::abort();
  }
};

template <typename TError, CResultLogger<TError> TResultLogger>
struct [[nodiscard("Result should be checked for errors")]] ResultBase<void, TError, TResultLogger>
    : public std::expected<void, TError> {
  using base = std::expected<void, TError>;
  using base::base;  // inherit constructors

  ResultBase(const std::expected<void, TError>& exp) : base(exp) {}        // NOLINT
  ResultBase(std::expected<void, TError>&& exp) : base(std::move(exp)) {}  // NOLINT

  void or_panic(ResultFmtWithLoc fmt_loc = {}) {
    if (this->has_value()) {
      return std::move(this->value());
    }
    TResultLogger::log_panic(fmt_loc.loc, this->error(), fmt_loc.value);
    std::abort();
  }
};

template <typename TError>
struct GenericResultLogger {
  static void log_panic(const std::source_location& l, const TError&, zstring_view msg = "") {
    if (msg.empty()) {
      Logger::instance().log(LogLevel::Err, false, l, "Program has crashed!");
    } else {
      Logger::instance().log(LogLevel::Err, false, l, "Program has crashed with message: \"{}\"", msg);
    }
  }
};

template <typename TType, typename TError>
using Result = ResultBase<TType, TError, GenericResultLogger<TError>>;

}  // namespace eray::util
