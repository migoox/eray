#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <filesystem>
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
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace vkren = eray::vkren;

class ComputerShaderApplication : public vkren::VulkanApplication {
 private:
  vkren::BufferResource vert_buffer_;
  vkren::BufferResource ind_buffer_;

  vkren::ImageResource txt_image_;
  vk::raii::ImageView txt_view_  = nullptr;
  vk::raii::Sampler txt_sampler_ = nullptr;

  static constexpr auto kViewportsCount = 4U;
  static constexpr auto kViewportSize   = 500U;

  vk::DescriptorSetLayout main_dsl_;
  vk::raii::PipelineLayout main_pipeline_layout_ = nullptr;
  vk::raii::Pipeline main_pipeline_              = nullptr;

 public:
  void on_init() override {
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    window().set_window_size(kViewportSize * kViewportsCount / 2U, kViewportSize * kViewportsCount / 2U);

    for (auto i = 0U; i < kViewportsCount; ++i) {
      viewports_[i].name = std::format("Viewport {}", i);
    }

    // == Render graph setup ===========================================================================================
    for (auto& viewport : viewports_) {
      auto msaa_color_attachment = render_graph().create_color_attachment(device(), kViewportSize, kViewportSize, false,
                                                                          vk::SampleCountFlagBits::e8);
      auto color_attachment      = render_graph().create_color_attachment(device(), kViewportSize, kViewportSize, true);
      auto depth_attachment      = render_graph().create_depth_attachment(device(), kViewportSize, kViewportSize, true,
                                                                          vk::SampleCountFlagBits::e8);

      viewport.render_pass = render_graph()
                                 .render_pass_builder(vk::SampleCountFlagBits::e8)
                                 .with_msaa_color_attachment(msaa_color_attachment, color_attachment)
                                 .with_depth_attachment(depth_attachment)
                                 .on_emit([this, &viewport](vkren::Device&, vk::CommandBuffer& cmd_buff) {
                                   this->record_render_pass(cmd_buff, viewport);
                                 })
                                 .build(kViewportSize, kViewportSize)
                                 .or_panic("Could not create render pass");

      render_graph().emplace_final_pass_dependency(color_attachment);

      viewport.color_attachment = color_attachment;
    }

    // == Buffers setup ================================================================================================
    {
      auto vb = VertexBuffer::create();

      auto vertices_region = eray::util::MemoryRegion{vb.vertices.data(), vb.vertices_size_bytes()};
      vert_buffer_         = vkren::BufferResource::create_vertex_buffer(device(), vertices_region.size_bytes())
                         .or_panic("Could not create the vertex buffer");
      vert_buffer_.write(vertices_region).or_panic("Could not fill the vertex buffer");

      auto indices_region = eray::util::MemoryRegion{vb.indices.data(), vb.indices_size_bytes()};
      ind_buffer_         = vkren::BufferResource::create_index_buffer(device(), indices_region.size_bytes())
                        .or_panic("Could not create a Vertex Buffer");
      ind_buffer_.write(indices_region).or_panic("Could not fill the index buffer");

      for (auto& viewport : viewports_) {
        vk::DeviceSize size_bytes = sizeof(UniformBufferObject);
        auto ubo = vkren::BufferResource::create_persistently_mapped_uniform_buffer(device(), size_bytes)
                       .or_panic("Could not create the uniform buffer");
        viewport.uniform_buffer_ = std::move(ubo.buffer);

        // Copying to uniform buffer each frame means that staging buffer makes no sense.
        viewport.uniform_buffer_mapped_ = ubo.mapped_data;
      }
    }

    // == Images setup =================================================================================================
    {
      auto img = eray::res::Image::load_from_path(eray::os::System::executable_dir() / "assets" / "cad.jpeg")
                     .or_panic("cad is not there :(");
      txt_image_ = vkren::ImageResource::create_texture(device(), vkren::ImageDescription::from(img))
                       .or_panic("Could not create a texture image");
      txt_image_.upload(img.memory_region()).or_panic("Could not upload the image");
      txt_view_ = txt_image_.create_image_view().or_panic("Could not create the image view");

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
      txt_sampler_ = vkren::Result(device().vk().createSampler(sampler_info)).or_panic("Could not create the sampler");
    }

    // == Descriptors setup ============================================================================================
    for (auto& viewport : viewports_) {
      auto result = vkren::DescriptorSetBuilder::create(device())
                        .with_binding(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
                        .with_binding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                        .build()
                        .or_panic("Could not create descriptor sets");

      viewport.render_pass_ds_ = std::move(result.descriptor_set);
      main_dsl_                = std::move(result.layout);

      auto binder = vkren::DescriptorSetBinder::create(device());
      binder.bind_buffer(0, viewport.uniform_buffer_.desc_buffer_info(), vk::DescriptorType::eUniformBuffer);
      binder.bind_combined_image_sampler(1, txt_view_, txt_sampler_, vk::ImageLayout::eShaderReadOnlyOptimal);
      binder.apply(viewport.render_pass_ds_);
      binder.clear();

      viewport.imgui_txt_ds_ = ImGui_ImplVulkan_AddTexture(
          static_cast<VkSampler>(vk::Sampler{txt_sampler_}),
          static_cast<VkImageView>(vk::ImageView{render_graph().attachment(viewport.color_attachment).view}),
          static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
    }

    // == Shaders + Graphics Pipeline ==================================================================================
    {
      auto main_binary =
          eray::res::SPIRVShaderBinary::load_from_path(eray::os::System::executable_dir() / "shaders" / "main.spv")
              .or_panic("Could not find main_sh.spv");
      auto main_shader_module =
          vkren::ShaderModule::create(device(), main_binary).or_panic("Could not create a main shader module");

      auto binding_desc = Vertex::binding_desc();
      auto attribs_desc = Vertex::attribs_desc();

      // All pipelines are the same for each viewport, so only one is created
      auto pipeline = vkren::GraphicsPipelineBuilder::create(render_graph(), viewports_[0].render_pass)
                          .with_shaders(main_shader_module.shader_module, main_shader_module.shader_module)
                          .with_input_state(binding_desc, attribs_desc)
                          .with_descriptor_set_layout(main_dsl_)
                          .with_depth_test()
                          .build(device())
                          .or_panic("Could not create a graphics pipeline");

      main_pipeline_        = std::move(pipeline.pipeline);
      main_pipeline_layout_ = std::move(pipeline.layout);
    }
  }

  void on_process(float /*delta*/) override {
    mark_frame_data_dirty();
    auto window_size = window().window_size();

    for (auto i = 0U; i < kViewportsCount; ++i) {
      auto t = std::chrono::duration<float>(time()).count();
      auto s = std::sin(t * 0.7F);
      s      = (s * s - 0.5F) * 90.F;

      auto axis         = eray::math::Vec3f::filled(0.F);
      axis.data[i % 3U] = 1.F;

      viewports_[i].ubo.model = eray::math::rotation_axis(eray::math::radians(s), axis);
      viewports_[i].ubo.view  = eray::math::translation(eray::math::Vec3f(0.F, 0.F, -4.F));
      viewports_[i].ubo.proj  = eray::math::perspective_vk_rh(
          eray::math::radians(80.0F), static_cast<float>(window_size.width) / static_cast<float>(window_size.height),
          0.01F, 10.F);
    }
  }

  void on_frame_prepare_sync(Duration /*delta*/) override {
    for (auto& viewport : viewports_) {
      memcpy(viewport.uniform_buffer_mapped_, &viewport.ubo, sizeof(viewport.ubo));
    }
  }

  void on_imgui() override {
    ImGui::DockSpaceOverViewport();
    for (auto i = 0U; i < kViewportsCount; ++i) {
      ImGui::PushID(static_cast<int>(i));
      ImGui::Begin(viewports_[i].name.c_str());
      ImGui::Image(viewports_[i].imgui_txt_ds_, ImVec2(kViewportSize, kViewportSize));
      ImGui::End();
      ImGui::PopID();
    }
  }

  void record_render_pass(vk::CommandBuffer& cmd_buff, ViewportInfo& viewport) {
    cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics, main_pipeline_);
    cmd_buff.bindVertexBuffers(0, vert_buffer_.vk_buffer(), {0});
    cmd_buff.bindIndexBuffer(ind_buffer_.vk_buffer(), 0, vk::IndexType::eUint16);
    cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, main_pipeline_layout_, 0, viewport.render_pass_ds_,
                                nullptr);
    cmd_buff.drawIndexed(12, 1, 0, 0, 0);
  }

  void on_destroy() override {
    for (auto& viewport : viewports_) {
      ImGui_ImplVulkan_RemoveTexture(viewport.imgui_txt_ds_);
    }
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
  auto app = eray::vkren::VulkanApplication::create<ComputerShaderApplication>();
  app.run();

  // == Cleanup ========================================================================================================
  eray::os::System::instance().terminate();

  return 0;
}
