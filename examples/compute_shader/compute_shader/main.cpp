#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <compute_shader/particle.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/os/system.hpp>
#include <liberay/res/image.hpp>
#include <liberay/res/shader.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/memory_region.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/util/try.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/app.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/buffer/ubo.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/descriptor.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/glfw/vk_glfw_window_creator.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/pipeline.hpp>
#include <liberay/vkren/render_graph.hpp>
#include <liberay/vkren/shader.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

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

  static constexpr uint32_t kWinWidth  = 800;
  static constexpr uint32_t kWinHeight = 600;

 public:
  void on_init() override {
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    window().set_window_size(800, 600);

    // Buffers setup
    auto particle_system =
        ParticleSystem::create_on_circle(static_cast<float>(kWinWidth) / static_cast<float>(kWinHeight));
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

    render_graph()
        .compute_pass_builder()
        .with_shader_storage(ssbo_handle_)
        .on_emit([this](vkren::Device&, vk::CommandBuffer& cmd_buff) {
          cmd_buff.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline_);
          cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline_layout_, 0, {compute_ds_}, {});
          cmd_buff.dispatch(ParticleSystem::kParticleCount / 256, 1, 1);
        })
        .build()
        .or_panic();

    render_graph().emplace_final_pass_storage_buffer_dependency(ssbo_handle_);
  }

  void on_process(float delta) override {
    ubo_cpu_.delta_time = delta;
    ubo_gpu_.mark_dirty();

    mark_frame_data_dirty();
  }

  void on_frame_prepare_sync(Duration /*delta*/) override { ubo_gpu_.sync(ubo_cpu_); }

  void on_record_graphics(vk::CommandBuffer cmd, uint32_t) override {
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    cmd.bindVertexBuffers(0, {render_graph().shader_storage_buffer(ssbo_handle_).buffer.vk_buffer()}, {0});
    cmd.draw(ParticleSystem::kParticleCount, 1, 0, 0);
  }
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
  auto app = eray::vkren::VulkanApplication::create<ComputeShaderApplication>();
  app.run();

  // == Cleanup ========================================================================================================
  eray::os::System::instance().terminate();

  return 0;
}
