#pragma once

#include <cstdint>
#include <liberay/util/enum_mapper.hpp>

namespace eray::os {

enum class Driver : uint8_t {
  OpenGL    = 0,
  Vulcan    = 1,
  DirectX11 = 2,
  DirectX12 = 3,
  _Count    = 4,  // NOLINT
};

constexpr auto kDriverName = util::StringEnumMapper<Driver>({
    {Driver::OpenGL, "OpenGL"},
    {Driver::Vulcan, "Vulcan"},
    {Driver::DirectX11, "DirectX11"},
    {Driver::DirectX12, "DirectX12"},
});

}  // namespace eray::os
