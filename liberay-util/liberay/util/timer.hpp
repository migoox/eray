#pragma once
#include <chrono>

namespace eray::util {

class Timer {
 public:
  Timer() : start_time_(std::chrono::high_resolution_clock::now()), end_time_(start_time_) {}

  void start() {
    start_time_ = std::chrono::high_resolution_clock::now();
    end_time_   = start_time_;
  }
  void stop() { end_time_ = std::chrono::high_resolution_clock::now(); }

  double measured_secs() const { return std::chrono::duration<double>(end_time_ - start_time_).count(); }
  double measured_mill() const { return std::chrono::duration<double, std::milli>(end_time_ - start_time_).count(); }
  double elapsed_mill() const {
    return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start_time_).count();
  }
  double elapsed_secs() const {
    return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time_).count();
  }

  std::string formatted_elapsed_mill() const { return std::format("Elapsed time: {:.3f} ms", elapsed_mill()); }
  std::string formatted_elapsed_secs() const { return std::format("Elapsed time: {:.3f} s", elapsed_secs()); }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
  std::chrono::time_point<std::chrono::high_resolution_clock> end_time_;
};

}  // namespace eray::util

namespace std {

// Custom formatter for Timer
template <>
struct formatter<eray::util::Timer> : formatter<std::string_view> {
  // Parse optional format specifiers: "s" (seconds) or "ms" (milliseconds)
  enum class FormatType { Milliseconds, Seconds } format_type = FormatType::Milliseconds;

  constexpr auto parse(format_parse_context& ctx) {
    const auto* it  = ctx.begin();
    const auto* end = ctx.end();

    if (it != end) {
      if (*it == 's') {
        format_type = FormatType::Seconds;
        ++it;
      } else if (*it == 'm') {
        // support "ms"
        ++it;
        if (it != end && *it == 's') {
          format_type = FormatType::Milliseconds;
          ++it;
        }
      }
    }

    // Return iterator to end of the format specifier
    return it;
  }

  auto format(const eray::util::Timer& timer, format_context& ctx) const {
    std::string out;
    if (format_type == FormatType::Seconds) {
      out = std::format("{:.3f} s", timer.elapsed_secs());
    } else {
      out = std::format("{:.3f} ms", timer.elapsed_mill());
    }

    return formatter<std::string_view>::format(out, ctx);
  }
};

}  // namespace std
