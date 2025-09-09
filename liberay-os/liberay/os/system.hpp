#pragma once

#include <deque>
#include <liberay/os/file_dialog.hpp>
#include <liberay/os/operating_system.hpp>
#include <liberay/os/rendering_api.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/os/window/window_creator.hpp>
#include <liberay/os/window_api.hpp>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/platform.hpp>
#include <liberay/util/result.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/zstring_view.hpp>
#include <memory>

namespace eray::os {

/**
 * @brief Singleton class that provides an abstraction over common operating system calls. Basing on the requested
 * graphics rendering API this class is capable of creating a window.
 *
 */
class System {
 public:
  System()                         = delete;
  System(const System&)            = delete;
  System& operator=(const System&) = delete;
  System(System&&)                 = delete;
  System& operator=(System&&)      = default;

  explicit System(std::unique_ptr<IWindowCreator>&& window_creator)
      : rendering_api_(window_creator->rendering_api()), window_creator_(std::move(window_creator)) {}

  /**
   * @brief Returns a singleton instance.
   *
   * @return System&
   */
  static System& instance() { return *instance_; }

  /**
   * @brief Initializes the System singleton. This call is required before using the `instance()` method.
   *
   * @param window_creator
   * @return Result<void, Error>
   */
  static Result<void, Error> init(std::unique_ptr<IWindowCreator>&& window_creator);

  /**
   * @brief Must be invoked at the end of the program.
   *
   * @warning Windows life time must not exceed the moment when this function is called.
   */
  void terminate();

  /**
   * @brief Returns a detected operating system enum value. This information is gathered at compile time.
   *
   * @return OperatingSystem
   */
  static constexpr OperatingSystem operating_system() {
#ifdef IS_LINUX
    return OperatingSystem::Linux;
#elif defined(IS_WINDOWS)
    return OperatingSystem::Windows;
#elif defined(IS_MACOS)
    return OperatingSystem::MacOS;
#else
    return OperatingSystem::Other;
#endif
  }

  /**
   * @brief Returns a detected operating system string name. This information is gathered at compile time.
   *
   * @return constexpr util::zstring_view
   */
  static constexpr util::zstring_view operating_system_name() { return kOperatingSystemName[operating_system()]; }

  /**
   * @brief Returns requested rendering API.
   *
   * @return RenderingAPI
   */
  RenderingAPI rendering_api() const { return rendering_api_; }

  /**
   * @brief Returns a string name of driver that is assumed when creating windows.
   *
   * @return util::zstring_view
   */
  util::zstring_view rendering_api_name() const { return kRenderingAPIName[rendering_api_]; }

  /**
   * @brief Returns window API backend.
   *
   * @return WindowAPI
   */
  WindowAPI window_api() const { return window_creator_->window_api(); }

  /**
   * @brief Returns window API backend string.
   *
   * @return WindowAPI
   */
  util::zstring_view window_api_name() const { return kWindowingAPIName[window_creator_->window_api()]; }

  /**
   * @brief Get the executable path of the application. The path is system agnostic.
   *
   * @warning MacOS is not supported yet. This function works on Linux and Windows.
   *
   * @return std::filesystem::path
   */
  static std::filesystem::path executable_path();

  /**
   * @brief Get the executable directory. Useful when loading assets. The path is system agnostic.
   *
   * @return std::filesystem::path
   */
  static std::filesystem::path executable_dir();

  /**
   * @brief Get current working directory path. The path is system agnostic. Equivalent of
   * `std::filesystem::current_path()`.
   *
   * @return std::filesystem::path
   */
  static std::filesystem::path current_working_dir();

  /**
   * @brief Convert the system agnostic path to UTF8 string. Since Windows paths are stored as wide chars, this function
   * is extremely useful when dealing with external libraries that expect UTF8 strings.
   *
   * @param path
   * @return std::string
   */
  static std::string path_to_utf8str(const std::filesystem::path& path);

  /**
   * @brief Convert the UTF8 string to a system agnostic path. Extremely usefull when dealing with external libraries.
   *
   * @param str_path
   * @return std::filesystem::path
   */
  static std::filesystem::path utf8str_to_path(util::zstring_view str_path);

  /**
   * @brief Creates a window and returns an unique pointer to the instance.
   * The window is valid until the `terminate` function is not called.
   *
   * @return util::Result<std::unique_ptr<IWindow>, WindowCreationError>
   */
  [[nodiscard]] util::Result<std::shared_ptr<Window>, Error> create_window();

  /**
   * @brief Creates a window and returns an unique pointer to the instance. Allows to specify window properties.
   * The window is valid until the `terminate` function is not called.
   *
   * @param props
   * @return util::Result<std::unique_ptr<Window>, WindowCreationError>
   */
  [[nodiscard]] util::Result<std::shared_ptr<Window>, Error> create_window(const WindowProperties& props);

  [[nodiscard]] static FileDialog& file_dialog() { return FileDialog::instance(); }

 private:
  RenderingAPI rendering_api_;
  std::unique_ptr<IWindowCreator> window_creator_;
  std::vector<std::shared_ptr<Window>> windows_;

  std::deque<std::function<void()>> deletors_;

  static std::unique_ptr<System> instance_;
};

}  // namespace eray::os
