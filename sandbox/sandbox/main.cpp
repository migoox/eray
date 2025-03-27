#include <functional>
#include <liberay/driver/glsl_shader.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/os/system.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/object_handle.hpp>
#include <liberay/util/observer_ptr.hpp>
#include <liberay/util/panic.hpp>
#include <print>

struct Test {
  int x = 5;
};

struct Test2 {
  int y = 5;
};

int main() {
  using namespace eray;  // NOLINT

  util::Logger::instance().add_scribe(std::make_unique<util::TerminalLoggerScribe>());

  driver::GLSLShaderManager sm;
  auto sh = util::unwrap_or_panic(sm.load_shader(os::System::executable_dir() / "assets" / "main.vert"));
  sh.set_ext_defi("TEST", "1.0");

  util::Logger::info("{}", sh.get_glsl());

  Test x{};
  Test y{};
  auto t = std::reference_wrapper<Test>(x);
  t      = std::reference_wrapper<Test>(y);

  auto h  = util::Handle<Test>(1, 1, 1);
  auto h2 = util::Handle<Test>(1, 1, 1);
  auto h3 = util::Handle<Test2>(1, 1, 1);

  if (util::AnyObjectHandle(h) == util::AnyObjectHandle(h3)) {
  }

  return 0;
}
