#include <expected>
#include <filesystem>
#include <liberay/os/error.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/os/window/window_props.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/path_utf8.hpp>
#include <liberay/util/platform.hpp>
#include <liberay/util/zstring_view.hpp>
#include <memory>

#ifdef IS_WINDOWS
#include <windows.h>
#endif

namespace eray::os {

std::unique_ptr<System> System::instance_ = nullptr;

Result<void, Error> System::init(std::unique_ptr<IWindowCreator>&& window_creator) {
  auto driver = window_creator->rendering_api();
  if constexpr (operating_system() == OperatingSystem::Linux) {
    if (driver != RenderingAPI::OpenGL && driver != RenderingAPI::Vulkan) {
      util::Logger::err("Requested driver ({}) that is not supported on Linux operating systems.",
                        kRenderingAPIName[driver]);
      return std::unexpected(Error{
          .msg  = std::format("Linux does not support requested driver {}", kRenderingAPIName[driver]),
          .code = ErrorCode::RenderingAPINotSupported{},
      });
    }
  } else if constexpr (operating_system() == OperatingSystem::MacOS) {
    if (driver != RenderingAPI::OpenGL && driver != RenderingAPI::Vulkan) {
      util::Logger::err("Requested driver ({}) that is not supported on MacOS.", kRenderingAPIName[driver]);
      return std::unexpected(Error{
          .msg  = std::format("Linux does not support requested driver {}", kRenderingAPIName[driver]),
          .code = ErrorCode::RenderingAPINotSupported{},
      });
    }
  }
  util::Logger::info("Requested driver: {}", kRenderingAPIName[driver]);

  instance_ = std::make_unique<System>(std::move(window_creator));
  instance_->deletors_.emplace_back([]() { instance_->window_creator_->terminate(); });

  return {};
}

util::Result<std::shared_ptr<Window>, Error> System::create_window() {
  auto window = create_window(WindowProperties{
      .title      = "Window",
      .vsync      = false,
      .fullscreen = false,
      .width      = 800,
      .height     = 600,
  });

  if (window) {
    windows_.emplace_back(*window);
    deletors_.emplace_back([window = windows_.back()]() { window->destroy(); });
  }

  return window;
}

util::Result<std::shared_ptr<Window>, Error> System::create_window(const WindowProperties& props) {
  return window_creator_->create_window(std::move(props));
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

void System::terminate() {
  for (auto& deletor : std::ranges::reverse_view(deletors_)) {
    deletor();
  }
  deletors_.clear();
  windows_.clear();
}

}  // namespace eray::os
