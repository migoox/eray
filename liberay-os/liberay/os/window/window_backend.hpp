#pragma once

#include <expected>
#include <liberay/os/driver.hpp>
#include <liberay/os/imgui_backend.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/os/window/window_props.hpp>
#include <liberay/util/ruleof.hpp>
#include <memory>

namespace eray::os {

/**
 * @brief An interface that represents a backend (e.g. GLFW) responsible for instantiating windows.
 */
class IWindowBackend {
 public:
  enum class WindowCreationError : uint8_t {
    FailedToInitializeDriverContext = 0,
  };

  virtual std::expected<std::unique_ptr<Window>, WindowCreationError> create_window(WindowProperties props) = 0;

  virtual ~IWindowBackend() = default;
};

}  // namespace eray::os
