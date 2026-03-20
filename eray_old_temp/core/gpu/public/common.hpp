#pragma once

#include <liberay/util/result.hpp>
#include <liberay/vkren/error.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace eray::vkren {

template <typename TError>
struct VulkanResultLogger {
  static void log_panic(const std::source_location& l, const TError& err, util::zstring_view msg = "") {
    if constexpr (std::is_same_v<TError, vk::Result>) {
      if (msg.empty()) {
        util::Logger::instance().log(util::LogLevel::Err, false, l, "Program has crashed due to a Vulkan error: {}",
                                     vk::to_string(err));
      } else if constexpr (std::is_same_v<TError, Error>) {
        if (err.vk_code != vk::Result::eSuccess) {
          util::Logger::instance().log(util::LogLevel::Err, false, l,
                                       "Program has crashed due to a Vulkan error: {}. Error message: {}",
                                       vk::to_string(err.vk_code), err.msg);
        } else {
          util::Logger::instance().log(util::LogLevel::Err, false, l, "Program has crashed. Error message: {}",
                                       err.msg);
        }
      } else {
        util::Logger::instance().log(util::LogLevel::Err, false, l,
                                     "Program has crashed due to a Vulkan error: {}. Message: \"{}\"",
                                     vk::to_string(err), msg);
      }
    } else {
      if (msg.empty()) {
        util::Logger::instance().log(util::LogLevel::Err, false, l, "Program has crashed!");
      } else if constexpr (std::is_same_v<TError, Error>) {
        if (err.vk_code != vk::Result::eSuccess) {
          util::Logger::instance().log(util::LogLevel::Err, false, l,
                                       "Program has crashed due to a Vulkan error: {}. Error message: {}. {}",
                                       vk::to_string(err.vk_code), err.msg, msg);
        } else {
          util::Logger::instance().log(util::LogLevel::Err, false, l, "Program has crashed. Error message: {}. {}",
                                       err.msg, msg);
        }
      } else {
        util::Logger::instance().log(util::LogLevel::Err, false, l, "Program has crashed with message: \"{}\"", msg);
      }
    }
  }
};

template <typename TType, typename TError>
using Result = util::ResultBase<TType, TError, VulkanResultLogger<TError>>;

template <typename TFlags, typename TFlagBits>
constexpr bool has_flag(TFlags lhs, TFlagBits rhs) {
  return (lhs & rhs) == rhs;
}

template <typename T>
using observer_ptr = T*;

}  // namespace eray::vkren
