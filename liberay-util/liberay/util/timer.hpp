#pragma once
#include <chrono>

namespace eray::util {

class Timer {
 public:
  void start() { start_time_ = end_time_ = std::chrono::time_point<std::chrono::high_resolution_clock>(); }
  void stop() { end_time_ = std::chrono::high_resolution_clock::now(); }
  double measured_secs() { return std::chrono::duration<double, std::milli>(end_time_ - start_time_).count(); }
  double measured_mill() { return std::chrono::duration<double, std::milli>(end_time_ - start_time_).count(); }
  double elapsed_mill() const {
    return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start_time_).count();
  }
  double elapsed_secs() const {
    return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start_time_).count();
  }

  std::string formatted_elapsed_mill() const { return std::format("Elapsed time: {}ms", elapsed_mill()); }
  std::string formatted_elapsed_secs() const { return std::format("Elapsed time: {}s", elapsed_secs()); }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
  std::chrono::time_point<std::chrono::high_resolution_clock> end_time_;
};

}  // namespace eray::util
