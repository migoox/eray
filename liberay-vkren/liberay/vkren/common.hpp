#pragma once

#include <liberay/util/result.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace eray::vkren {

template <typename TError>
struct VulkanResultLogger {
  static void log_crash(const std::source_location& l, const TError& err, util::zstring_view msg = "") {
    if constexpr (std::is_same_v<TError, vk::Result>) {
      if (msg.empty()) {
        util::Logger::instance().log(util::LogLevel::Err, false, l, "Program has crashed due to a Vulkan error: {}",
                                     vk::to_string(err));
      } else {
        util::Logger::instance().log(util::LogLevel::Err, false, l,
                                     "Program has crashed due to a Vulkan error: {}. Message: \"{}\"",
                                     vk::to_string(err), msg);
      }
    } else {
      if (msg.empty()) {
        util::Logger::instance().log(util::LogLevel::Err, false, l, "Program has crashed!");
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

}  // namespace eray::vkren
