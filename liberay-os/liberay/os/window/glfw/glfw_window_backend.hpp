#pragma once
#include <liberay/os/window/window_backend.hpp>
#include <liberay/util/ruleof.hpp>

namespace eray::os {

class GLFWWindowBackend : public IWindowBackend {
 public:
  GLFWWindowBackend() = delete;
  ERAY_DELETE_COPY_AND_MOVE(GLFWWindowBackend)

  ~GLFWWindowBackend() override;

  enum class BackendCreationError : uint8_t {
    DriverIsNotSupported = 0,
    InitializationError  = 1,
  };

  [[nodiscard]] static std::expected<std::unique_ptr<GLFWWindowBackend>, BackendCreationError> create(Driver driver);

  [[nodiscard]] std::expected<std::unique_ptr<Window>, WindowCreationError> create_window(
      WindowProperties props) override;

 private:
  explicit GLFWWindowBackend(Driver driver);

 private:
  Driver driver_;
};

}  // namespace eray::os
