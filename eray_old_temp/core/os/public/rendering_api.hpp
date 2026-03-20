#pragma once

#include <cstdint>
#include <liberay/util/enum_mapper.hpp>

namespace eray::os {

enum class RenderingAPI : uint8_t {
  OpenGL    = 0,
  Vulkan    = 1,
  DirectX11 = 2,
  DirectX12 = 3,
  _Count    = 4,  // NOLINT
};

constexpr auto kRenderingAPIName = util::StringEnumMapper<RenderingAPI>({
    {RenderingAPI::OpenGL, "OpenGL"},
    {RenderingAPI::Vulkan, "Vulcan"},
    {RenderingAPI::DirectX11, "DirectX11"},
    {RenderingAPI::DirectX12, "DirectX12"},
});

}  // namespace eray::os
