#pragma once
#include <__targetname__/camera.hpp>
#include <__targetname__/mesh.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/vkren/app.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/render_graph.hpp>

namespace __namespace__ {

struct UniformBufferObject {
  alignas(16) eray::math::Mat4f model;
  alignas(16) eray::math::Mat4f view;
  alignas(16) eray::math::Mat4f proj;
  alignas(16) eray::math::Vec4f light_dir;
  alignas(16) eray::math::Vec4f camera_pos;
};

class __class__ : public eray::vkren::VulkanApplication {
 private:
  vk::raii::Sampler txt_sampler_ = nullptr;

  static constexpr auto kViewportSizeX = 1280U;
  static constexpr auto kViewportSizeY = 720U;

  static constexpr auto kInitWindowSizeX = 1280U;
  static constexpr auto kInitWindowSizeY = 720U;

  struct Viewport {
    eray::vkren::RenderPassAttachmentHandle color_attachment;
    eray::vkren::RenderPassHandle render_pass;
    VkDescriptorSet imgui_txt_ds;
  } viewport_;

  struct Light {
    float pitch_deg;
    float yaw_deg;
  } light_{};

  std::unique_ptr<Camera> camera_ = nullptr;

  vk::DescriptorSetLayout main_dsl_;
  vk::raii::PipelineLayout main_pipeline_layout_ = nullptr;
  vk::raii::Pipeline main_pipeline_              = nullptr;
  vk::DescriptorSet main_ds_;

  Mesh mesh_;
  UniformBufferObject ubo_;
  eray::vkren::BufferResource ubo_gpu_;
  void* ubo_map_;

  bool on_viewport_      = false;
  bool use_orthographic_ = true;

 public:
  void on_init() override;
  void on_process(float delta) override;
  void on_frame_prepare_sync(Duration) override;
  void on_imgui() override;
  void on_destroy() override;
  void on_process_physics(float delta) override;
  void record_render_pass(vk::CommandBuffer& cmd_buff);
};

}  // namespace __namespace__
