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
  struct ExtensionNotSupported {
    std::string extension;
  };
  struct ProfileNotSupported {
    std::string name;
    uint32_t version;
  };
  struct ValidationLayerNotSupported {};
  struct PhysicalDeviceNotSufficient {};
  struct SurfaceCreationFailure {};
  struct MemoryMappingFailure {};
  struct MemoryMappingNotSupported {};
  struct NotATransferDestination {};
  struct SwapChainImageAcquireFailure {};
  struct PresentationFailure {};
  struct InvalidRenderPass {};
  struct InvalidRenderGraph {};
  struct FileError {};
  struct ParserError {};

  using Enum = std::variant<         //
      VulkanObjectCreationFailure,   //
      NoSuitableMemoryTypeFailure,   //
      MemoryAllocationFailure,       //
      ExtensionNotSupported,         //
      ValidationLayerNotSupported,   //
      ProfileNotSupported,           //
      PhysicalDeviceNotSufficient,   //
      SurfaceCreationFailure,        //
      MemoryMappingFailure,          //
      MemoryMappingNotSupported,     //
      SwapChainImageAcquireFailure,  //
      PresentationFailure,           //
      InvalidRenderPass,             //
      InvalidRenderGraph,            //
      FileError,                     //
      ParserError                    //
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

  /**
   * @brief Vulkan API error code. If no Vulkan API error occurred, this member stores `vk::Result::eSuccess`.
   *
   */
  vk::Result vk_code = vk::Result::eSuccess;

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

}  // namespace eray::vkren
