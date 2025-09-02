#include <expected>
#include <filesystem>
#include <liberay/os/driver.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/glfw/glfw_window_backend.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/os/window/window_backend.hpp>
#include <liberay/os/window/window_props.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/path_utf8.hpp>
#include <liberay/util/platform.hpp>
#include <liberay/util/zstring_view.hpp>
#include <optional>

#ifdef IS_WINDOWS
#include <windows.h>
#endif

namespace eray::os {

std::optional<RenderingAPI> System::requested_driver_ = std::nullopt;

std::expected<void, System::DriverRequestError> System::request_driver(RenderingAPI driver) {
  std::unique_ptr<IWindowBackend> win_backend;
  if constexpr (operating_system() == OperatingSystem::Linux) {
    if (driver != RenderingAPI::OpenGL && driver != RenderingAPI::Vulcan) {
      util::Logger::err("Requested driver ({}) that is not supported on linux systems.", kDriverName[driver]);
      return std::unexpected(DriverRequestError::OperatingSystemDoesNotSupportRequestedDriver);
    }
  } else if constexpr (operating_system() == OperatingSystem::MacOS) {
    if (driver != RenderingAPI::OpenGL) {
      util::Logger::err("Requested driver ({}) that is not supported on MacOS.", kDriverName[driver]);
      return std::unexpected(DriverRequestError::OperatingSystemDoesNotSupportRequestedDriver);
    }
  }
  util::Logger::info("Requested driver: {}", kDriverName[driver]);
  requested_driver_ = driver;
  return {};
}

System::System(RenderingAPI driver) : driver_(driver) {
  if (driver_ == RenderingAPI::OpenGL || driver_ == RenderingAPI::Vulcan) {
    auto result = GLFWWindowBackend::create(driver_);
    if (result.has_value()) {
      window_backend_ = std::move(*result);
    } else {
      util::panic("Failed to initialize GLFW backend");
    }
  } else if (driver_ == RenderingAPI::DirectX11 || driver_ == RenderingAPI::DirectX12) {
    util::panic("Requested driver ({}) that is not supported on Windows yet.", kDriverName[driver_]);
  }
}

std::expected<std::unique_ptr<Window>, IWindowBackend::WindowCreationError> System::create_window() {
  return create_window({
      .title         = "Window",
      .vsync         = false,
      .fullscreen    = false,
      .size          = math::Vec2i(800, 600),
      .has_valid_pos = false,
  });
}

std::expected<std::unique_ptr<Window>, IWindowBackend::WindowCreationError> System::create_window(
    WindowProperties props) {
  return window_backend_->create_window(std::move(props));
}

std::filesystem::path System::executable_path() {
#ifdef IS_WINDOWS
  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  return std::filesystem::path(buffer);
#else
  return std::filesystem::read_symlink("/proc/self/exe");
#endif
  // TODO(migoox): Add MacOS support and update the doxygen comment.
}

std::filesystem::path System::executable_dir() { return executable_path().parent_path(); }

std::filesystem::path System::current_working_dir() { return std::filesystem::current_path(); }

std::string System::path_to_utf8str(const std::filesystem::path& path) { return util::path_to_utf8str(path); }

std::filesystem::path System::utf8str_to_path(util::zstring_view str_path) { return util::utf8str_to_path(str_path); }

}  // namespace eray::os
