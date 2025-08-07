#pragma once

#include <vulkan/vulkan_core.h>

#include <expected>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

class ISurfaceCreator {
 public:
  struct SurfaceCreationError {};

  virtual ~ISurfaceCreator() = default;

  virtual std::expected<VkSurfaceKHR, SurfaceCreationError> create(vk::raii::Instance&);
};

}  // namespace eray::vkren
