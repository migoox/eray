#pragma once
#include <liberay/glren/glfw/gl_glfw_swap_chain.hpp>
#include <liberay/glren/vertex_array.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/os/window/window.hpp>
#include <liberay/util/ruleof.hpp>
#include <memory>

namespace eray::glren {

using namespace std::chrono_literals;

class Application {
 public:
  Application();

  virtual ~Application() = default;

  ERAY_DELETE_COPY_AND_MOVE(Application)

  using Duration = std::chrono::nanoseconds;
  using Clock    = std::chrono::high_resolution_clock;

  virtual void run();

 protected:
  /**
   * @brief Invoked with a delta time between two frames
   *
   * @param delta
   */
  virtual void render_gui(Duration delta);

  /**
   * @brief Invoked with a delta time between two frames
   *
   * @param delta
   */
  virtual void render(Duration delta);

  /**
   * @brief Invoked with a fixed step delta (basing on tps)
   *
   * @param delta
   */
  virtual void update(Duration delta);

 private:
  bool on_closed(const os::WindowClosedEvent& closed_event);

 protected:
  static constexpr Duration kTickTime = 16666us;  // 60 TPS = 16.6(6) ms/t
  Duration time_                      = 0ns;
  std::uint16_t fps_                  = 0;
  std::uint16_t tps_                  = 0;

  bool running_   = true;
  bool minimized_ = false;

  GLFWSwapChain swap_chain_ = GLFWSwapChain(nullptr);

  std::shared_ptr<os::Window> window_;
};

}  // namespace eray::glren
