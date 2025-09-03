#pragma once

#include <cstdint>
#include <liberay/util/enum_mapper.hpp>

namespace eray::os {

enum class WindowAPI : uint8_t {
  GLFW   = 0,
  WinAPI = 1,
  _Count = 2,  // NOLINT
};

constexpr auto kWindowingAPIName = util::StringEnumMapper<WindowAPI>({
    {WindowAPI::GLFW, "GLFW"},
    {WindowAPI::WinAPI, "WinAPI"},
});

}  // namespace eray::os
