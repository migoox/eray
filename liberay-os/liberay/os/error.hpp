#pragma once

#include <liberay/util/result.hpp>
#include <string>
#include <variant>

namespace eray::os {

class ErrorCode {
 public:
  struct WindowBackendNotSupported {};
  struct WindowBackendCreationFailure {};
  struct WindowBackendFailure {};
  struct RenderingAPIInitializationFailure {};
  struct RenderingAPINotSupported {};

  using Enum = std::variant<              //
      WindowBackendNotSupported,          //
      WindowBackendCreationFailure,       //
      WindowBackendFailure,               //
      RenderingAPIInitializationFailure,  //
      RenderingAPINotSupported            //
      >;
};

struct Error {
  /**
   * @brief Short error summary.
   *
   */
  std::string msg;

  /**
   * @brief Error code with optional context info.
   *
   */
  ErrorCode::Enum code;

  template <typename TErrorCode>
  bool has_code() const {
    return std::holds_alternative<TErrorCode>(code);
  }

  template <typename TErrorCode>
  TErrorCode& get_code() {
    return std::get<TErrorCode>(code);
  }

  template <typename TErrorCode>
  const TErrorCode& get_code() const {
    return std::get<TErrorCode>(code);
  }
};

template <typename TType, typename TError>
using Result = util::Result<TType, TError>;

}  // namespace eray::os
