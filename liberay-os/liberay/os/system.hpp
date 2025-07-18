#pragma once

#include <cstdint>
#include <expected>
#include <liberay/os/driver.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/os/window/window_backend.hpp>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/platform.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/zstring_view.hpp>
#include <optional>

#include "liberay/os/file_dialog.hpp"

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

/**
 * @brief Singleton class that provides an abstraction over common operating system calls. Basing on the requested
 * graphics driver this class is capable of creating a window.
 *
 */
class System {
 public:
  System() = delete;
  ERAY_DELETE_COPY_AND_MOVE(System)

  enum class DriverRequestError : uint8_t {
    OperatingSystemDoesNotSupportRequestedDriver = 0,
  };

  /**
   * @brief Allows to request a driver. Basing on the driver a proper window backend is used later, e.g. GLFW for
   * OpenGL. This function must be called before first call to `System`s `instance()` -- it has no effect otherwise. If
   * this function is not called, OpenGL will be used by default.
   *
   * @param driver
   * @return std::expected<void, DriverRequestError>
   */
  static std::expected<void, DriverRequestError> request_driver(Driver driver);

  static System& instance() {
    static auto system = System(requested_driver_.value_or(Driver::OpenGL));
    return system;
  }

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
   * @brief Driver that is assumed when creating windows.
   *
   * @return Driver
   */
  std::optional<Driver> driver() { return driver_; }

  /**
   * @brief Returns a string name of driver that is assumed when creating windows.
   *
   * @return util::zstring_view
   */
  std::optional<util::zstring_view> driver_name() { return kDriverName[driver_]; }

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
   *
   * @return std::expected<std::unique_ptr<IWindow>, WindowCreationError>
   */
  [[nodiscard]] std::expected<std::unique_ptr<Window>, IWindowBackend::WindowCreationError> create_window();

  /**
   * @brief Creates a window and returns an unique pointer to the instance. Allows to specify window properties.
   *
   * @param window_properties
   * @return std::expected<std::unique_ptr<Window>, WindowCreationError>
   */
  [[nodiscard]] std::expected<std::unique_ptr<Window>, IWindowBackend::WindowCreationError> create_window(
      WindowProperties props);

  [[nodiscard]] static FileDialog& file_dialog() { return FileDialog::instance(); }

 private:
  explicit System(Driver driver);

 private:
  Driver driver_;
  std::unique_ptr<IWindowBackend> window_backend_;

  static std::optional<Driver> requested_driver_;
};

}  // namespace eray::os
