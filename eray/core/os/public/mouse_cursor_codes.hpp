#pragma once

#include <cstdint>
#include <liberay/util/enum_mapper.hpp>

namespace eray::os {

enum class CursorMode : uint8_t {
  Normal   = 0,
  Hidden   = 1,
  Disabled = 2,
  Captured = 3,
  _Count   = 4  // NOLINT
};

constexpr util::StringEnumMapper<CursorMode> kMouseCursorModeNames({
    {CursorMode::Normal, "Normal"},      //
    {CursorMode::Hidden, "Hidden"},      //
    {CursorMode::Captured, "Captured"},  //
    {CursorMode::Disabled, "Disabled"},  //
});

constexpr util::zstring_view mouse_cursor_code(CursorMode code) { return kMouseCursorModeNames[code]; }

}  // namespace eray::os
