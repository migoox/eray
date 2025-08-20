#pragma once

#include <string>
#include <variant>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace eray::vkren {

class ErrorCode {
 public:
  struct VulkanObjectCreationFailure {};
  struct NoSuitableMemoryTypeFailure {};
  struct MemoryAllocationFailure {};
  struct ExtensionNotSupportedFailure {
    std::string extension;
  };
  struct ValidationLayerNotSupportedFailure {};
  struct PhysicalDeviceNotSufficient {};
  struct SurfaceCreationFailure {};

  using Enum = std::variant<               //
      VulkanObjectCreationFailure,         //
      NoSuitableMemoryTypeFailure,         //
      MemoryAllocationFailure,             //
      ExtensionNotSupportedFailure,        //
      ValidationLayerNotSupportedFailure,  //
      PhysicalDeviceNotSufficient,         //
      SurfaceCreationFailure               //
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
  std::variant<ErrorCode::Enum> code;

  /**
   * @brief Vulkan API error code. If no Vulkan API error occurred, this member stores `vk::Result::eSuccess`.
   *
   */
  vk::Result vk_code = vk::Result::eSuccess;
};

}  // namespace eray::vkren
