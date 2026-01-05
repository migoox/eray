#include <__targetname__/app.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/os/rendering_api.hpp>
#include <liberay/os/system.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/vkren/app.hpp>
#include <liberay/vkren/glfw/vk_glfw_window_creator.hpp>

using Logger = eray::util::Logger;
using System = eray::os::System;

int main() {
  // == Setup singletons ===============================================================================================
  Logger::instance().init();
  Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());

  auto window_creator =
      eray::os::VulkanGLFWWindowCreator::create().or_panic("Could not create a Vulkan GLFW window creator");
  System::init(std::move(window_creator)).or_panic("Could not initialize Operating System API");

  // == Application ====================================================================================================
  {
    auto app =
        eray::vkren::VulkanApplication::create<__namespace__::__class__>(eray::vkren::VulkanApplicationCreateInfo{
            .app_name = "__class__",
            .vsync    = false,
        });
    app.run();
  }

  // == Cleanup ========================================================================================================
  eray::os::System::instance().terminate();

  return 0;
}
