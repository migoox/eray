#pragma once

#include <cstdint>
#include <liberay/util/enum_mapper.hpp>

namespace eray::os {

enum class OperatingSystem : uint8_t {
  Linux   = 0,
  Windows = 1,
  MacOS   = 2,
  Other   = 3,
  _Count  = 4,  // NOLINT
};

constexpr auto kOperatingSystemName = util::StringEnumMapper<OperatingSystem>({
    {OperatingSystem::Linux, "Linux"},
    {OperatingSystem::Windows, "Windows"},
    {OperatingSystem::MacOS, "MacOS"},
    {OperatingSystem::Other, "Unknown operating system"},
});

}  // namespace eray::os
