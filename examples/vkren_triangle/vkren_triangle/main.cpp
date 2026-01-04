#include <imgui/imgui.h>

#include <liberay/math/mat.hpp>
#include <liberay/os/rendering_api.hpp>
#include <liberay/os/system.hpp>
#include <liberay/os/window/input_codes.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/memory_region.hpp>
#include <liberay/vkren/app.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/glfw/vk_glfw_window_creator.hpp>
#include <liberay/vkren/pipeline.hpp>
#include <liberay/vkren/shader.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

using Logger = eray::util::Logger;
using System = eray::os::System;

namespace vkren = eray::vkren;
namespace math  = eray::math;
namespace util  = eray::util;
namespace os    = eray::os;

struct Vertex {
  math::Vec2f pos;
  math::Vec3f color;
};

class VkRenTriangleApplication : public vkren::VulkanApplication {
 private:
  vkren::BufferResource vbo_;
  vkren::Pipeline pipeline_;

 public:
  void on_init() override {
    {
      auto vertices = std::array{
          Vertex{
              .pos   = math::Vec2f{0.0F, 0.5F},
              .color = math::Vec3f{1.F, 0.F, 0.F},
          },
          Vertex{
              .pos   = math::Vec2f{0.5F, -0.5F},
              .color = math::Vec3f{0.F, 1.F, 0.F},
          },
          Vertex{
              .pos   = math::Vec2f{-0.5F, -0.5F},
              .color = math::Vec3f{0.F, 0.F, 1.F},
          },
      };
      auto mem = util::MemoryRegion{vertices.data(), vertices.size() * sizeof(Vertex)};

      vbo_ = vkren::BufferResource::create_vertex_buffer(*ctx().device, mem.size_bytes())
                 .or_panic("Could not create the vertex buffer");
      vbo_.write(mem).or_panic();
    }

    auto binding_desc = vk::VertexInputBindingDescription{
        .binding   = 0,
        .stride    = sizeof(Vertex),
        .inputRate = vk::VertexInputRate::eVertex,
    };

    auto attribs_desc = std::array{
        vk::VertexInputAttributeDescription{
            .location = 0,
            .binding  = 0,
            .format   = vk::Format::eR32G32Sfloat,
            .offset   = offsetof(Vertex, pos),
        },
        vk::VertexInputAttributeDescription{
            .location = 1,
            .binding  = 0,
            .format   = vk::Format::eR32G32B32Sfloat,
            .offset   = offsetof(Vertex, color),
        },
    };

    auto shader =
        vkren::ShaderModule::load_from_path(device(), System::executable_dir() / "shaders" / "main.spv").or_panic();

    pipeline_ = vkren::GraphicsPipelineBuilder::create(swap_chain())
                    .with_shaders(shader.shader_module)
                    .with_input_state(binding_desc, attribs_desc)
                    .with_primitive_topology(vk::PrimitiveTopology::eTriangleList)
                    .with_cull_mode(vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise)
                    .build(device())
                    .or_panic("Could not create the pipeline");
  }

  void on_imgui() override {
    ImGui::Begin("Test Window");
    ImGui::Text("FPS: %d", fps());
    ImGui::End();
  }

  void on_process_physics(float) override {
    if (input().is_input_captured()) {
      return;
    }

    if (input().is_key_just_pressed(os::KeyCode::W)) {
      Logger::info("just pressed W");
    }
    if (input().is_key_just_released(os::KeyCode::W)) {
      Logger::info("just released W");
    }
    if (input().is_mouse_btn_just_pressed(os::MouseBtnCode::MouseButtonLeft)) {
      Logger::info("just pressed Left");
    }
    if (input().is_key_pressed(os::KeyCode::D)) {
      Logger::info("pressed D");
    }
    if (input().delta_mouse_scroll_y() > 0.F) {
      Logger::info("scroll: {}", input().delta_mouse_scroll_y());
    }
    if (input().delta_mouse_pos_x() > 0.F) {
      Logger::info("delta pos x: {}", input().delta_mouse_pos_x());
    }
  }

  void on_record_graphics(vk::CommandBuffer cmd, uint32_t) override {
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_.pipeline);
    cmd.bindVertexBuffers(0, {vbo_.vk_buffer()}, {0});
    cmd.draw(3, 1, 0, 0);
  }
};

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
        eray::vkren::VulkanApplication::create<VkRenTriangleApplication>(eray::vkren::VulkanApplicationCreateInfo{
            .app_name    = "VkRenTriangle",
            .enable_msaa = true,
            .vsync       = false,
        });
    app.run();
  }  // Destroy app

  // == Cleanup ========================================================================================================
  eray::os::System::instance().terminate();

  return 0;
}
