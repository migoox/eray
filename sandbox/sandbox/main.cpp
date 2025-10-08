#include <liberay/glren/app.hpp>
#include <liberay/glren/glfw/gl_glfw_window_creator.hpp>
#include <liberay/os/rendering_api.hpp>
#include <liberay/os/system.hpp>
#include <liberay/util/logger.hpp>

int main() {
  using Logger = eray::util::Logger;

  Logger::instance().init();
  Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());

  auto window_creator =
      eray::os::OpenGLGLFWWindowCreator::create().or_panic("Could not create GLFW OpenGL window creator");
  eray::os::System::init(std::move(window_creator)).or_panic("Could not initialize Operating System API");

  auto app = eray::glren::Application();
  app.run();

  eray::os::System::instance().terminate();

  return 0;
}
