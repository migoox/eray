#include <liberay/util/logger.hpp>
#include <version/version.hpp>

int main() {
  using Logger = eray::util::Logger;

  Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());
  Logger::instance().set_abs_build_path(ERAY_BUILD_ABS_PATH);

  Logger::info("Hello World!");

  return 0;
}
