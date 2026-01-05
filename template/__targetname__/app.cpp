#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imgui/imgui_internal.h>

#include <__targetname__/app.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/os/window/events/event.hpp>
#include <liberay/util/memory_region.hpp>
#include <liberay/vkren/app.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/pipeline.hpp>
#include <liberay/vkren/shader.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace __namespace__ {

namespace vkren = eray::vkren;
namespace math  = eray::math;

void __class__::on_init() {
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  window().set_window_size(kInitWindowSizeX, kInitWindowSizeY);

  camera_ = std::make_unique<Camera>(use_orthographic_, math::radians(70.F),
                                     static_cast<float>(kInitWindowSizeX) / static_cast<float>(kInitWindowSizeY),
                                     0.001F, 1000.F);
  camera_->set_distance_from_origin(5.F);
  light_ = Light{
      .pitch_deg = -60.F,
      .yaw_deg   = 180.F,
  };

  // == Render graph setup =============================================================================================
  auto msaa_color_attachment = render_graph().create_color_attachment(device(), kViewportSizeX, kViewportSizeY, false,
                                                                      vk::SampleCountFlagBits::e8);
  auto color_attachment      = render_graph().create_color_attachment(device(), kViewportSizeX, kViewportSizeY, true);
  auto depth_attachment      = render_graph().create_depth_attachment(device(), kViewportSizeX, kViewportSizeY, true,
                                                                      vk::SampleCountFlagBits::e8);

  viewport_.render_pass =
      render_graph()
          .render_pass_builder(vk::SampleCountFlagBits::e8)
          .with_msaa_color_attachment(msaa_color_attachment, color_attachment)
          .with_depth_attachment(depth_attachment)
          .on_emit([this](vkren::Device&, vk::CommandBuffer& cmd_buff) { this->record_render_pass(cmd_buff); })
          .build(kViewportSizeX, kViewportSizeY)
          .or_panic("Could not create render pass");

  render_graph().emplace_final_pass_dependency(color_attachment);

  viewport_.color_attachment = color_attachment;

  // == Buffers setup ================================================================================================
  {
    mesh_ = Mesh::create_box(device(), math::Vec3f{1.F, 0.F, 0.F}, math::Mat4f::identity());

    vk::DeviceSize size_bytes = sizeof(UniformBufferObject);
    auto ubo                  = vkren::BufferResource::create_persistently_mapped_uniform_buffer(device(), size_bytes)
                   .or_panic("Could not create the uniform buffer");
    ubo_gpu_ = std::move(ubo.buffer);
    ubo_map_ = ubo.mapped_data;
  }

  // == Images setup =================================================================================================
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
    txt_sampler_ = vkren::Result(device().vk().createSampler(sampler_info)).or_panic("Could not create the sampler");
  }

  // == Descriptors setup ============================================================================================
  auto ds_binder = vkren::DescriptorSetBinder::create(device());
  {
    auto main_ds = vkren::DescriptorSetBuilder::create(device())
                       .with_binding(vk::DescriptorType::eUniformBuffer,
                                     vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
                       .build()
                       .or_panic("Could not create descriptor sets");

    main_ds_  = std::move(main_ds.descriptor_set);
    main_dsl_ = std::move(main_ds.layout);

    ds_binder.bind_buffer(0, ubo_gpu_.desc_buffer_info(), vk::DescriptorType::eUniformBuffer);
    ds_binder.apply_and_clear(main_ds_);
  }

  viewport_.imgui_txt_ds = ImGui_ImplVulkan_AddTexture(
      static_cast<VkSampler>(vk::Sampler{txt_sampler_}),
      static_cast<VkImageView>(vk::ImageView{render_graph().attachment(viewport_.color_attachment).view}),
      static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));

  // == Shaders + Graphics Pipeline ==================================================================================
  {
    auto main_binary =
        eray::res::SPIRVShaderBinary::load_from_path(eray::os::System::executable_dir() / "shaders" / "mesh_phong.spv")
            .or_panic("Could not find main_sh.spv");
    auto main_shader_module =
        vkren::ShaderModule::create(device(), main_binary).or_panic("Could not create a main shader module");

    auto binding_desc = mesh_.binding_desc;  // boxes are the same
    auto attribs_desc = mesh_.attribs_desc;

    // All pipelines are the same for each viewport, so only one is created
    auto pipeline = vkren::GraphicsPipelineBuilder::create(render_graph(), viewport_.render_pass)
                        .with_shaders(main_shader_module.shader_module, main_shader_module.shader_module)
                        .with_input_state(binding_desc, attribs_desc)
                        .with_descriptor_set_layout(main_dsl_)
                        .with_blending()
                        .build(device())
                        .or_panic("Could not create a graphics pipeline");

    main_pipeline_        = std::move(pipeline.pipeline);
    main_pipeline_layout_ = std::move(pipeline.layout);
  }
}

void __class__::on_process(float) {
  auto const vp_aspect_ratio = static_cast<float>(kViewportSizeX) / static_cast<float>(kViewportSizeY);

  camera_->set_aspect_ratio(vp_aspect_ratio);
  auto light_dir = math::rotation_y(math::radians(light_.yaw_deg)) * math::rotation_x(math::radians(light_.pitch_deg)) *
                   math::Vec4f(0.F, 0.F, -1.F, 0.F);  // start dir points at -Z (into the screen)

  ubo_.view       = camera_->view_matrix();
  ubo_.proj       = camera_->proj_matrix();
  ubo_.light_dir  = light_dir;
  ubo_.camera_pos = math::Vec4f{camera_->pos(), 1.F};
  ubo_.model      = math::Mat4f::identity();

  mark_frame_data_dirty();
}

void __class__::on_frame_prepare_sync(Duration /*delta*/) { memcpy(ubo_map_, &ubo_, sizeof(UniformBufferObject)); }

void __class__::on_imgui() {
  ImGui::DockSpaceOverViewport();
  on_viewport_ = false;

  ImGui::Begin("Viewport", nullptr,
               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
  auto win_size = ImGui::GetWindowSize();
  ImGui::SetScrollY(0.F);
  ImGui::SetScrollX(0.F);
  ImGui::SetCursorPosX((win_size.x - kViewportSizeX) / 2);
  ImGui::Image(viewport_.imgui_txt_ds, ImVec2(kViewportSizeX, kViewportSizeY));
  on_viewport_ = on_viewport_ || ImGui::IsItemHovered();
  ImGui::End();

  if (ImGui::Begin("Settings")) {
    if (ImGui::Button("Reset camera")) {
      camera_->set_pitch(0.F);
      camera_->set_yaw(0.F);
      camera_->set_origin(math::Vec3f::zeros());
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("Orthographic", &use_orthographic_)) {
      camera_->set_orthographic(use_orthographic_);
    }

    ImGui::End();
  }
}

void __class__::record_render_pass(vk::CommandBuffer& cmd_buff) {
  cmd_buff.bindPipeline(vk::PipelineBindPoint::eGraphics, main_pipeline_);
  cmd_buff.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, main_pipeline_layout_, 0, main_ds_, nullptr);
  mesh_.render(cmd_buff);
}

void __class__::on_destroy() { ImGui_ImplVulkan_RemoveTexture(viewport_.imgui_txt_ds); }

void __class__::on_process_physics(float delta) { camera_->on_process_physics(input(), delta); }

}  // namespace __namespace__
