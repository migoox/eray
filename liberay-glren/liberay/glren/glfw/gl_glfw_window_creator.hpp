#pragma once
#include <liberay/os/error.hpp>
#include <liberay/os/rendering_api.hpp>
#include <liberay/os/window/window_creator.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/ruleof.hpp>

namespace eray::os {

class OpenGLGLFWWindowCreator : public IWindowCreator {
 public:
  OpenGLGLFWWindowCreator() = default;
  ERAY_DELETE_COPY_AND_MOVE(OpenGLGLFWWindowCreator)

  ~OpenGLGLFWWindowCreator() override;

  [[nodiscard]] static Result<std::unique_ptr<IWindowCreator>, Error> create();
  [[nodiscard]] Result<std::unique_ptr<Window>, Error> create_window(const WindowProperties& props) override;
  [[nodiscard]] RenderingAPI rendering_api() override { return RenderingAPI::Vulkan; }
  [[nodiscard]] WindowAPI window_api() override { return WindowAPI::GLFW; }
};

}  // namespace eray::os
