#pragma once

// NOLINTBEGIN
/**
 * @brief Use when the function returns std::expected<void, Err>
 *
 */
#define TRY(expr)                                        \
  if (const auto result = (expr); !result.has_value()) { \
    return std::unexpected(result.error());              \
  }

/**
 * @brief Use when the function returns std::expected<Type, Err>
 *
 */
#define TRY_UNWRAP_DEFINE(var, expr)                 \
  auto _##var##_result = (expr);                     \
  if (!_##var##_result.has_value()) {                \
    return std::unexpected(_##var##_result.error()); \
  }                                                  \
  auto var = std::move(_##var##_result.value());

/**
 * @brief Use when the function returns std::expected<Type, Err>
 *
 */
#define TRY_UNWRAP_ASSIGN(var, expr)                 \
  auto _##var##_result = (expr);                     \
  if (!_##var##_result.has_value()) {                \
    return std::unexpected(_##var##_result.error()); \
  }                                                  \
  var = std::move(_##var##_result.value());
// NOLINTEND

// Statement expressions is a gcc extension, not a standard :(
//
// #define BEAUTIFUL_TRY_UNWRAP(m)                                \
//   ({                                                           \
//     auto res = (m);                                            \
//     if (!res.has_value()) return std::unexpected(res.error()); \
//     std::move(res.value());                                    \
//   })
