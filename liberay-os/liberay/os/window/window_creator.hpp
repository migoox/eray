#pragma once

#include <liberay/os/error.hpp>
#include <liberay/os/rendering_api.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/os/window/window_props.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/result.hpp>
#include <liberay/util/ruleof.hpp>
#include <memory>

namespace eray::os {

/**
 * @brief An interface that represents a backend (e.g. GLFW) responsible for instantiating windows.
 */
class IWindowCreator {
 public:
  virtual util::Result<std::unique_ptr<Window>, Error> create_window(const WindowProperties& props) = 0;
  virtual RenderingAPI rendering_api()                                                              = 0;
  virtual WindowAPI window_api()                                                                    = 0;

  virtual ~IWindowCreator() = default;
};

}  // namespace eray::os
