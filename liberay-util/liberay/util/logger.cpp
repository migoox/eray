#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <liberay/util/logger.hpp>
#include <liberay/util/platform.hpp>
#include <liberay/util/zstring_view.hpp>
#include <memory>
#include <mutex>
#include <print>
#include <regex>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#ifndef IS_UNIX
#include <windows.h>
#endif

namespace eray::util {

LoggerScribe::LoggerScribe(LogLevel max_level) : max_level_(max_level) {}

TerminalLoggerScribe::TerminalLoggerScribe(bool use_stderr, LogLevel max_level)
    : LoggerScribe(max_level), std_stream_(use_stderr ? &std::cerr : &std::cout) {}

namespace {

#ifdef IS_UNIX
void begin_unix_terminal(std::string_view style) { std::print("\033{}", style); }
void end_unix_terminal() { std::print("\033[0m\n"); }
#else
WORD begin_win_terminal(WORD attributes) {
  HANDLE h_console = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO console_info;
  WORD old_win_terminal_attributes = 0;

  if (GetConsoleScreenBufferInfo(h_console, &console_info)) {
    old_win_terminal_attributes = console_info.wAttributes;
  }

  SetConsoleTextAttribute(h_console, attributes);
  return old_win_terminal_attributes;
}

void end_win_terminal(WORD old_attributes) {
  HANDLE h_console = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(h_console, old_attributes);
  std::print("\n");
}
#endif

void print_msg(std::ostream& stream, std::string_view prefix, std::string_view usr_fmt, std::format_args usr_args,
               const std::chrono::time_point<std::chrono::system_clock>& time_point, std::string_view file_name,
               const std::source_location& location, LogLevel level) {
  // Print logger message prefix with date
  auto local_time =
      std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds>(time_point)};
  std::print(stream, "[{0:{2}} {1:%T}]: ", prefix, local_time, kMaxLogPrefixSize);

  // Print location
  if (level < LogLevel::Success) {
#ifdef NDEBUG
    std::print(stream, "`{0}`: ", location.function_name());
#else
    std::print(stream, "{0}({1}:{2}) `{3}`: ", file_name, location.line(), location.column(), location.function_name());
#endif
  }

  // Print message provided by the user
  std::vprint_unicode(stream, usr_fmt, usr_args);
}

}  // namespace

void TerminalLoggerScribe::vlog(std::string_view usr_fmt, std::format_args usr_args,
                                const std::chrono::time_point<std::chrono::system_clock>& time_point,
                                std::string_view file_path, const std::source_location& location, LogLevel level,
                                bool is_debug_msg) {
#ifdef IS_DEBUG
  if (is_debug_msg) {
#ifdef IS_UNIX
    begin_unix_terminal("[34m");  // Blue
#else
    WORD old_win_terminal_attributes = begin_win_terminal(FOREGROUND_BLUE);
#endif

    print_msg(*std_stream_, kDebugLogPrefix, usr_fmt, usr_args, time_point, file_path, location, level);

#ifdef IS_UNIX
    end_unix_terminal();
#else
    end_win_terminal(old_win_terminal_attributes);
#endif
    return;
  }
#endif

  if (level > max_level_) {
    return;
  }

#ifdef IS_UNIX
  if (level == LogLevel::Err) {
    begin_unix_terminal("[1;31m");  // Red bold
  } else if (level == LogLevel::Warn) {
    begin_unix_terminal("[1;33m");  // Yellow bold
  } else if (level == LogLevel::Success) {
    begin_unix_terminal("[;32m");  // Green
  } else {
    begin_unix_terminal("[;37m");  // White
  }
#else
  WORD old_win_terminal_attributes = 0;
  if (level == LogLevel::Err) {
    old_win_terminal_attributes = begin_win_terminal(FOREGROUND_RED);
  } else if (level == LogLevel::Warn) {
    old_win_terminal_attributes = begin_win_terminal(FOREGROUND_RED | FOREGROUND_GREEN);
  } else if (level == LogLevel::Success) {
    old_win_terminal_attributes = begin_win_terminal(FOREGROUND_GREEN);
  } else {
    old_win_terminal_attributes = begin_win_terminal(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
  }
#endif

  print_msg(*std_stream_, get_log_prefix(level), usr_fmt, usr_args, time_point, file_path, location, level);

#ifdef IS_UNIX
  end_unix_terminal();
#else
  end_win_terminal(old_win_terminal_attributes);
#endif
}

RotatedFileLoggerScribe::RotatedFileLoggerScribe(std::filesystem::path base_path, size_t max_backups,
                                                 LogLevel max_level)
    : LoggerScribe(max_level), file_stream_(std::nullopt), base_path_(std::move(base_path)), max_backups_(max_backups) {
  namespace fs = std::filesystem;
  namespace ch = std::chrono;

  static const std::regex kFileNameRegexp(R"(dori_logs_(\d{4})-(\d{2})-(\d{2})T(\d{2})-(\d{2})-(\d{2}).txt)");
  static constexpr zstring_view kDateFormat           = "dori_logs_%Y-%m-%dT%H-%M-%S.txt";
  static constexpr zstring_view kOutputFileDateFormat = "dori_logs_{0:%FT%H-%M-%S}.txt";

  if (!fs::exists(base_path_)) {
    Logger::err(
        "File logger scribe could not open the directory \"{0}\": "
        "Directory does not exists.",
        base_path_.string());
    return;
  }

  if (!fs::is_directory(base_path_)) {
    Logger::err(
        "File logger scribe could not open the directory \"{0}\": "
        "Provided path is not a directory.",
        base_path_.string());
    return;
  }

  using file_tp = std::tuple<fs::path, ch::system_clock::time_point>;

  auto cmp = [](const file_tp& a, const file_tp& b) -> bool { return std::get<1>(a) > std::get<1>(b); };
  std::set<file_tp, decltype(cmp)> curr_files;

  for (const auto& entry : fs::directory_iterator(base_path_)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    auto curr_name = entry.path().filename().string();

    if (std::regex_match(curr_name, kFileNameRegexp)) {
      std::tm tm = {};
      std::stringstream ss(curr_name);
      ss >> std::get_time(&tm, kDateFormat.c_str());

      curr_files.insert(std::make_tuple(entry.path(), ch::system_clock::from_time_t(std::mktime(&tm))));
    }
  }

  if (curr_files.size() > max_backups_ - 1) {
    auto it = curr_files.begin();
    std::advance(it, max_backups_ - 1);

    for (; it != curr_files.end(); ++it) {
      if (fs::remove(std::get<0>(*it))) {
        Logger::info("Deleted old log backup file \"{}\".", std::get<0>(*it).filename().string());
      } else {
        Logger::warn("Could not delete old log backup file \"{}\".", std::get<0>(*it).filename().string());
      }
    }
  }

  auto local_time = std::chrono::zoned_time{std::chrono::current_zone(),
                                            std::chrono::floor<std::chrono::seconds>(ch::system_clock::now())};
  auto file_path  = base_path_;
  file_path.append(std::format(kOutputFileDateFormat, local_time));

  file_stream_.emplace(file_path.string());
  if (!file_stream_->is_open()) {
    Logger::err("File logger scribe could not create/open a file \"{0}\"", file_path.string());
  }
}

void RotatedFileLoggerScribe::vlog(std::string_view usr_fmt, std::format_args usr_args,
                                   const std::chrono::time_point<std::chrono::system_clock>& time_point,
                                   std::string_view file_path, const std::source_location& location, LogLevel level,
                                   bool is_debug_msg) {
  if (!file_stream_.has_value() || !file_stream_->is_open()) {
    return;
  }

#ifdef NDEBUG
  if (level <= max_level_) {
    print_msg(file_stream_.value(), get_log_prefix(level), usr_fmt, usr_args, time_point, file_path, location, level);
    std::print(file_stream_.value(), "\n");
  }
#else
  if (is_debug_msg) {
    print_msg(file_stream_.value(), kDebugLogPrefix, usr_fmt, usr_args, time_point, file_path, location, level);
    std::print(file_stream_.value(), "\n");
    return;
  }

  if (level <= max_level_) {
    print_msg(file_stream_.value(), get_log_prefix(level), usr_fmt, usr_args, time_point, file_path, location, level);
    std::print(file_stream_.value(), "\n");
  }
#endif
}

Logger::Logger() : file_name_start_pos_(0) {}

void Logger::add_scribe(std::unique_ptr<LoggerScribe> scribe) {
  const std::lock_guard lock(mutex_);

  scribes_.push_back(std::move(scribe));
}

void Logger::set_abs_build_path(const std::filesystem::path& abs_build_path) {
  const std::lock_guard lock(mutex_);

  auto logger_path =
      std::filesystem::path{std::source_location::current().file_name()}.lexically_normal().generic_string();
  auto proj_abs_path = abs_build_path.lexically_normal().generic_string();

  if (logger_path.starts_with(proj_abs_path)) {
    file_name_start_pos_ = proj_abs_path.size() + 1;
  }
}

}  // namespace eray::util
