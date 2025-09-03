#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <source_location>
#include <string_view>
#include <vector>

namespace eray::util {

enum class LogLevel : uint8_t {
  Err     = 0,
  Warn    = 1,
  Success = 2,
  Info    = 3,
  _Count  = 4  // NOLINT
};

static constexpr std::array<std::string_view, static_cast<uint32_t>(LogLevel::_Count)> kLogPrefixes = {"ERROR", "WARN",
                                                                                                       "SUCC", "INFO"};
static constexpr std::string_view kDebugLogPrefix                                                   = "DEBUG";
static constexpr uint32_t kMaxLogPrefixSize                                                         = 5;

inline std::string_view log_prefix(LogLevel level) { return kLogPrefixes[static_cast<uint32_t>(level)]; }

class LoggerScribe {
 public:
  explicit LoggerScribe(LogLevel max_level);

  LoggerScribe(const LoggerScribe&)            = delete;
  LoggerScribe& operator=(const LoggerScribe&) = delete;
  virtual ~LoggerScribe()                      = default;

  virtual void vlog(std::string_view usr_fmt, std::format_args usr_args,
                    const std::chrono::time_point<std::chrono::system_clock>& time_point, std::string_view file_path,
                    const std::source_location& location, LogLevel level, bool is_debug_msg) = 0;

 protected:
  const LogLevel max_level_;
};

/**
 * @brief Logs messages to stdout or stderr. Provides message coloring support for both
 * unix and windows platforms.
 *
 */
class TerminalLoggerScribe : public LoggerScribe {
 public:
  explicit TerminalLoggerScribe(bool use_stderr = false, LogLevel max_level = LogLevel::Info);

  void vlog(std::string_view usr_fmt, std::format_args usr_args,
            const std::chrono::time_point<std::chrono::system_clock>& time_point, std::string_view file_path,
            const std::source_location& location, LogLevel level, bool is_debug_msg) override;

 private:
  std::ostream* std_stream_;
};

/**
 * @brief Logs messages to files in the specified directory. If the number of log files
 * exceeds the maximum number of backups the older ones will be deleted.
 *
 */
class RotatedFileLoggerScribe : public LoggerScribe {
 public:
  explicit RotatedFileLoggerScribe(std::filesystem::path base_path, size_t max_backups,
                                   LogLevel max_level = LogLevel::Info);

  void vlog(std::string_view usr_fmt, std::format_args usr_args,
            const std::chrono::time_point<std::chrono::system_clock>& time_point, std::string_view file_path,
            const std::source_location& location, LogLevel level, bool is_debug_msg) override;

 private:
  std::optional<std::ofstream> file_stream_;
  std::filesystem::path base_path_;
  size_t max_backups_;
};

/**
 * @brief Singleton thread-safe class that forwards messages to the provided
 * `LoggerScribe`s.
 *
 */
class Logger {
 public:
  Logger();
  ~Logger() = default;

  struct FormatWithLocation {
    const char* value;
    std::source_location loc;

    FormatWithLocation(const char* s,  // NOLINT
                       const std::source_location& l = std::source_location::current())
        : value(s), loc(l) {}
  };

  template <typename... Args>
  static void err(FormatWithLocation fmt_loc, const Args&... args) {
    instance().log(LogLevel::Err, false, fmt_loc.loc, fmt_loc.value, args...);
  }

  template <typename... Args>
  static void warn(FormatWithLocation fmt_loc, const Args&... args) {
    instance().log(LogLevel::Warn, false, fmt_loc.loc, fmt_loc.value, args...);
  }

  template <typename... Args>
  static void info(FormatWithLocation fmt_loc, const Args&... args) {
    instance().log(LogLevel::Info, false, fmt_loc.loc, fmt_loc.value, args...);
  }

  template <typename... Args>
  static void succ(FormatWithLocation fmt_loc, const Args&... args) {
    instance().log(LogLevel::Success, false, fmt_loc.loc, fmt_loc.value, args...);
  }

  template <typename... Args>
  static void debug(FormatWithLocation fmt_loc, const Args&... args) {
#ifdef IS_DEBUG
    instance().log(LogLevel::Info, true, fmt_loc.loc, fmt_loc.value, args...);
#else
    // Silence the unused parameters warnings when in release
    (void)fmt_loc;
    ((void)args, ...);
#endif
  }

  template <typename... Args>
  void log(const LogLevel level, const bool debug, const std::source_location& location, const std::string_view fmt,
           const Args&... args) {
    const std::lock_guard lock(mutex_);
    const auto file_path = std::string_view(location.file_name() + file_name_start_pos_);
    const auto now       = std::chrono::system_clock::now();
    for (const auto& scribe : scribes_) {
      scribe->vlog(fmt, std::make_format_args(args...), now, file_path, location, level, debug);
    }
  }

  void add_scribe(std::unique_ptr<LoggerScribe> scribe);
  void set_abs_build_path(const std::filesystem::path& abs_build_path);

  static Logger& instance() {
    static Logger instance;
    return instance;
  }

 private:
  std::vector<std::unique_ptr<LoggerScribe>> scribes_;
  std::mutex mutex_;
  size_t file_name_start_pos_;
};

}  // namespace eray::util
