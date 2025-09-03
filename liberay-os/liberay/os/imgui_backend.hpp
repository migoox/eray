#pragma once

#include <expected>
#include <liberay/os/rendering_api.hpp>

namespace eray::os {

class ImGuiBackend {
 public:
  enum class ImGuiBackendCreationError : uint8_t {
    DriverNotSupported = 0,
  };

  /**
   * @brief This function must be called after dispatcher initialization!
   *
   * @param window
   */
  virtual void init_driver(void* window) = 0;

  virtual void new_frame()          = 0;
  virtual void generate_draw_data() = 0;
  virtual void render_draw_data()   = 0;

  virtual ~ImGuiBackend() = default;
};

class ImGuiGLFWBackend : public ImGuiBackend {
 public:
  ImGuiGLFWBackend() = delete;
  ~ImGuiGLFWBackend();

  void init_driver(void* window) override;

  void new_frame() override;
  void generate_draw_data() override;
  void render_draw_data() override;

  [[nodiscard]] static std::expected<std::unique_ptr<ImGuiGLFWBackend>, ImGuiBackendCreationError> create(
      RenderingAPI driver);

 private:
  explicit ImGuiGLFWBackend(RenderingAPI driver);
  RenderingAPI driver_;
};

}  // namespace eray::os
