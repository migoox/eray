#pragma once
#include <gtest/gtest.h>

#include <liberay/math/mat.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/math/vec.hpp>

// Helper function to compare two vectors with a tolerance.
template <std::size_t N, eray::math::CFloatingPoint T>
::testing::AssertionResult AreGLMVectorsNear(const char* expected_expr, const char* actual_expr,
                                             const char* epsilon_expr, const eray::math::Vec<N, T>& expected,
                                             const eray::math::Vec<N, T>& actual, const T epsilon) {
  if (eray::math::eps_neq(expected, actual, epsilon)) {
    return ::testing::AssertionFailure() << "Expected equality with tolerance " << epsilon_expr
                                         << " of these vectors:\n\t" << expected_expr
                                         << "\n\t\tWhich is: " << std::format("{}", expected) << "\n\t" << actual_expr
                                         << "\n\t\tWhich is: " << std::format("{}", actual) << "\n";
  }

  return ::testing::AssertionSuccess();
}

// Helper function to compare two matrices with a tolerance.
template <std::size_t M, std::size_t N, eray::math::CFloatingPoint T>
::testing::AssertionResult AreGLMMatricesNear(const char* expected_expr, const char* actual_expr,
                                              const char* epsilon_expr, const eray::math::Mat<M, N, T>& expected,
                                              const eray::math::Mat<M, N, T>& actual, const T epsilon) {
  for (std::size_t i = 0; i < N; ++i) {
    if (eray::math::eps_neq(expected[i], actual[i], epsilon)) {
      return ::testing::AssertionFailure()
             << "Expected equality with tolerance " << epsilon_expr << " of these matrices:\n\t" << expected_expr
             << "\n\t\tWhich is: " << std::format("{}", expected) << "\n\t" << actual_expr
             << "\n\t\tWhich is: " << std::format("{}", actual) << "\n";
    }
  }
  return ::testing::AssertionSuccess();
}

// Helper function to compare two quaternions with a tolerance.
template <eray::math::CFloatingPoint T>
::testing::AssertionResult AreGLMQuaternionRotationsNear(const char* expected_expr, const char* actual_expr,
                                                         const char* epsilon_expr, const eray::math::Quat<T>& expected,
                                                         const eray::math::Quat<T>& actual, const T epsilon) {
  if (std::abs(std::abs(eray::math::dot(expected, actual)) - static_cast<T>(1)) >= epsilon) {
    return ::testing::AssertionFailure() << "Expected equality with tolerance " << epsilon_expr
                                         << " of these rotations:\n\t" << expected_expr
                                         << "\n\t\tWhich is: " << std::format("{}", expected) << "\n\t" << actual_expr
                                         << "\n\t\tWhich is: " << std::format("{}", actual) << "\n"
                                         << "|q1.q2|=" << std::format("{}", std::abs(eray::math::dot(expected, actual)))
                                         << " NOTE: two quaternions represent the same rotation iff |q1.q2|=1\n";
  }

  return ::testing::AssertionSuccess();
}

// Macro for glm vector comparison.
#define EXPECT_VEC_NEAR(expected, actual, epsilon) EXPECT_PRED_FORMAT3(AreGLMVectorsNear, expected, actual, epsilon)
#define ASSERT_VEC_NEAR(expected, actual, epsilon) ASSERT_PRED_FORMAT3(AreGLMVectorsNear, expected, actual, epsilon)

// Macro for glm matrix comparison.
#define EXPECT_MAT_NEAR(expected, actual, epsilon) EXPECT_PRED_FORMAT3(AreGLMMatricesNear, expected, actual, epsilon)
#define ASSERT_MAT_NEAR(expected, actual, epsilon) ASSERT_PRED_FORMAT3(AreGLMMatricesNear, expected, actual, epsilon)

// Macro for glm quaternion comparison.
#define EXPECT_ROT_NEAR(expected, actual, epsilon) \
  EXPECT_PRED_FORMAT3(AreGLMQuaternionRotationsNear, expected, actual, epsilon)
#define ASSERT_ROT_NEAR(expected, actual, epsilon) \
  ASSERT_PRED_FORMAT3(AreGLMQuaternionRotationsNear, expected, actual, epsilon)
