#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>

#include <compute_shader/particle.hpp>
#include <liberay/os/system.hpp>
#include <liberay/vkren/app.hpp>
#include <liberay/vkren/buffer/ubo.hpp>
#include <liberay/vkren/glfw/vk_glfw_window_creator.hpp>
#include <liberay/vkren/pipeline.hpp>
#include <liberay/vkren/render_graph.hpp>
#include <liberay/vkren/shader.hpp>

namespace vkren = eray::vkren;
namespace util  = eray::util;

class ComputeShaderApplication : public vkren::VulkanApplication {
 private:
  vkren::ShaderStorageHandle ssbo_handle_;
  vkren::MappedUniformBuffer<UniformBufferObject> ubo_gpu_;
  UniformBufferObject ubo_cpu_;

  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::PipelineLayout compute_pipeline_layout_  = nullptr;
  vk::raii::Pipeline graphics_pipeline_              = nullptr;
  vk::raii::Pipeline compute_pipeline_               = nullptr;

  vk::DescriptorSetLayout compute_ds_layout_;
  vk::DescriptorSet compute_ds_;

  struct Viewport {
    VkDescriptorSet imgui_txt_ds_;
    vk::raii::Sampler txt_sampler_ = nullptr;
    vkren::RenderPassHandle render_pass;
    vkren::RenderPassAttachmentHandle color_attachment;
  } viewport_;

  static constexpr uint32_t kWinWidth  = 1280;
  static constexpr uint32_t kWinHeight = 720;

  static constexpr uint32_t kViewportWidth  = 800;
  static constexpr uint32_t kViewportHeight = 600;

 public:
  void on_init() override {
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    window().set_window_size(kWinWidth, kWinHeight);

    // Buffers setup
    auto particle_system =
        ParticleSystem::create_on_circle(static_cast<float>(kViewportWidth) / static_cast<float>(kViewportHeight));
    auto region =
        util::MemoryRegion{particle_system.particles.data(), particle_system.particles.size() * sizeof(Particle)};
    ssbo_handle_ =
        render_graph().create_shader_storage_buffer(device(), region, vk::BufferUsageFlagBits::eVertexBuffer);

    ubo_gpu_            = vkren::MappedUniformBuffer<UniformBufferObject>::create(device()).or_panic();
    ubo_cpu_.delta_time = 0.F;

    // Descriptor set
    {
      auto result = vkren::DescriptorSetBuilder::create(device())
                        .with_binding(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute)
                        .with_binding(vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute)
                        .build()
                        .or_panic();
      compute_ds_layout_ = result.layout;
      compute_ds_        = result.descriptor_set;

      auto binder = vkren::DescriptorSetBinder::create(device());
      binder.bind_buffer(0, ubo_gpu_.desc_buffer_info(), vk::DescriptorType::eUniformBuffer);
      binder.bind_buffer(1, render_graph().shader_storage_buffer(ssbo_handle_).buffer.desc_buffer_info(),
                         vk::DescriptorType::eStorageBuffer);
      binder.apply(compute_ds_);
    }

    // Pipelines setup
    {
      auto particle_shader_module =
          vkren::ShaderModule::load_from_path(device(), eray::os::System::executable_dir() / "shaders" / "particle.spv")
              .or_panic("Could not create a compute shader module");

      auto pipeline = vkren::ComputePipelineBuilder::create()
                          .with_descriptor_set_layout(compute_ds_layout_)
                          .with_shader(particle_shader_module.shader_module)
                          .build(device())
                          .or_panic("Could not create a compute pipeline");

      compute_pipeline_        = std::move(pipeline.pipeline);
      compute_pipeline_layout_ = std::move(pipeline.layout);
    }

    {
      auto main_shader_module =
          vkren::ShaderModule::load_from_path(device(), eray::os::System::executable_dir() / "shaders" / "main.spv")
              .or_panic("Could not create a main shader module");

      auto binding_desc = ParticleSystem::binding_desc();
      auto attribs_desc = ParticleSystem::attribs_desc();

      auto pipeline = vkren::GraphicsPipelineBuilder::create(swap_chain())
                          .with_shaders(main_shader_module.shader_module, main_shader_module.shader_module)
                          .with_polygon_mode(vk::PolygonMode::eFill)
                          .with_cull_mode(vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise)
                          .with_input_state(binding_desc, attribs_desc)
                          .with_primitive_topology(vk::PrimitiveTopology::ePointList)
                          .build(device())
                          .or_panic("Could not create a graphics pipeline");

      graphics_pipeline_        = std::move(pipeline.pipeline);
      graphics_pipeline_layout_ = std::move(pipeline.layout);
    }

    // ImGui texture
    {
      auto pdev_props   = device().physical_device().getProperties();
      auto sampler_info = vk::SamplerCreateInfo{
          .magFilter        = vk::Filter::eLinear,
          .minFilter        = vk::Filter::eLinear,
          .mipmapMode       = vk::SamplerMipmapMode::eLinear,
          .addressModeU     = vk::SamplerAddressMode::eRepeat,
          .addressModeV     = vk::SamplerAddressMode::eRepeat,
          .addressModeW     = vk::SamplerAddressMode::eRepeat,
          .mipLodBias       = 0.0F,
          .anisotropyEnable = vk::True,
          .maxAnisotropy    = pdev_props.limits.maxSamplerAnisotropy,
          .compareEnable    = vk::False,
          .compareOp        = vk::CompareOp::eAlways,
          .minLod           = 0.F,
          .maxLod           = vk::LodClampNone,
      };
      viewport_.txt_sampler_ =
          vkren::Result(device().vk().createSampler(sampler_info)).or_panic("Could not create the sampler");
    }

    // Render graph setup
    {
      // Pass #1: Compute pass that updates particles
      render_graph()
          .compute_pass_builder()
          .with_shader_storage(ssbo_handle_)
          .on_emit([this](vkren::Device&, vk::CommandBuffer& cmd_buff) {
            cmd_buff.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline_);
            cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline_layout_, 0, {compute_ds_},
                                        {});
            cmd_buff.dispatch(ParticleSystem::kParticleCount / 256, 1, 1);
          })
          .build()
          .or_panic();

      // Pass #2: Render pass that draws particles into a texture
      auto msaa_color_attachment = render_graph().create_color_attachment(device(), kViewportWidth, kViewportHeight,
                                                                          false, swap_chain().msaa_sample_count());
      viewport_.color_attachment =
          render_graph().create_color_attachment(device(), kViewportWidth, kViewportHeight, true);

      viewport_.render_pass =
          render_graph()
              .render_pass_builder(swap_chain().msaa_sample_count())
              .with_msaa_color_attachment(msaa_color_attachment, viewport_.color_attachment)
              .on_emit([this](vkren::Device&, vk::CommandBuffer& cmd) {
                cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
                cmd.bindVertexBuffers(0, {render_graph().shader_storage_buffer(ssbo_handle_).buffer.vk_buffer()}, {0});
                cmd.draw(ParticleSystem::kParticleCount, 1, 0, 0);
              })

              // Wait for particles until they are ready
              .with_buffer_dependency(
                  ssbo_handle_, vk::PipelineStageFlagBits2::eFragmentShader | vk::PipelineStageFlagBits2::eVertexShader)

              .build(kViewportWidth, kViewportHeight)
              .or_panic("Could not create render pass");

      // Pass #3: Final pass that renders imgui along with the viewport texture

      // Wait for the viewport texture until it's ready
      render_graph().emplace_final_pass_dependency(viewport_.color_attachment);
    }

    viewport_.imgui_txt_ds_ = ImGui_ImplVulkan_AddTexture(
        static_cast<VkSampler>(vk::Sampler{viewport_.txt_sampler_}),
        static_cast<VkImageView>(vk::ImageView{render_graph().attachment(viewport_.color_attachment).view}),
        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
  }

  void on_process(float delta) override {
    ubo_cpu_.delta_time = delta;
    ubo_gpu_.mark_dirty();

    mark_frame_data_dirty();
  }

  void on_frame_prepare_sync(Duration /*delta*/) override { ubo_gpu_.sync(ubo_cpu_); }

  void on_imgui() override {
    ImGui::Begin("Viewport");
    ImGui::Text("FPS: %d", fps());
    ImGui::Image(viewport_.imgui_txt_ds_, ImVec2(kViewportWidth, kViewportHeight));
    ImGui::End();
  }

  void on_destroy() override { ImGui_ImplVulkan_RemoveTexture(viewport_.imgui_txt_ds_); }
};

int main() {
  // == Setup singletons ===============================================================================================
  using Logger = eray::util::Logger;
  using System = eray::os::System;

  Logger::instance().init();
  Logger::instance().add_scribe(std::make_unique<eray::util::TerminalLoggerScribe>());
  auto window_creator =
      eray::os::VulkanGLFWWindowCreator::create().or_panic("Could not create a Vulkan GLFW window creator");
  System::init(std::move(window_creator)).or_panic("Could not initialize Operating System API");

  // == Application ====================================================================================================
  auto app = eray::vkren::VulkanApplication::create<ComputeShaderApplication>(eray::vkren::VulkanApplicationCreateInfo{
      .vsync = false,
  });
  app.run();

  // == Cleanup ========================================================================================================
  eray::os::System::instance().terminate();

  return 0;
}
