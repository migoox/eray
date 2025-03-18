#include <liberay/driver/glsl_shader.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/os/system.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <print>

int main() {
  using namespace eray;  // NOLINT

  util::Logger::instance().add_scribe(std::make_unique<util::TerminalLoggerScribe>());

  driver::GLSLShaderManager sm;
  auto sh = util::unwrap_or_panic(sm.load_shader(os::System::executable_dir() / "assets" / "main.vert"));
  sh.set_ext_defi("TEST", "1.0");

  util::Logger::info("{}", sh.get_glsl());

  return 0;
}
