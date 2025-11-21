#include <GLFW/glfw3.h>
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

struct Vertex {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;

  Vec3 pos;
  Vec3 color;
  Vec2 tex_coord;

  static vk::VertexInputBindingDescription binding_desc() {
    return vk::VertexInputBindingDescription{
        // Index of the binding in the array of bindings
        .binding = 0,
        .stride  = sizeof(Vertex),

        // VK_VERTEX_INPUT_RATE_VERTEX: move to the next data entry after each vertex
        // VK_VERTEX_INPUT_RATE_INSTANCE: move to the next data entry after each instance (instanced rendering)
        .inputRate = vk::VertexInputRate::eVertex,
    };
  }

  static auto attribs_desc() {
    return std::array{
        vk::VertexInputAttributeDescription{
            // References the location directive of the input in the vertex shader
            .location = 0,
            .binding  = 0,
            .format   = vk::Format::eR32G32B32Sfloat,
            .offset   = offsetof(Vertex, pos),
        },
        vk::VertexInputAttributeDescription{
            .location = 1,
            .binding  = 0,
            .format   = vk::Format::eR32G32B32Sfloat,
            .offset   = offsetof(Vertex, color),
        },
        vk::VertexInputAttributeDescription{
            .location = 2,
            .binding  = 0,
            .format   = vk::Format::eR32G32Sfloat,
            .offset   = offsetof(Vertex, tex_coord),
        },
    };
  }
};

struct VertexBuffer {
  using Vec2 = eray::math::Vec2f;
  using Vec3 = eray::math::Vec3f;

  static VertexBuffer create() {
    // interleaving vertex attributes
    return VertexBuffer{
        .vertices =
            std::vector<Vertex>{
                Vertex{.pos = Vec3{0.5F, 0.5F, 0.F}, .color = Vec3{1.0F, 0.0F, 0.0F}, .tex_coord = Vec2{1.F, 1.F}},
                Vertex{.pos = Vec3{0.5F, -0.5F, 0.F}, .color = Vec3{0.0F, 1.0F, 0.0F}, .tex_coord = Vec2{1.F, 0.F}},
                Vertex{.pos = Vec3{-0.5F, -0.5F, 0.F}, .color = Vec3{0.0F, 0.0F, 1.0F}, .tex_coord = Vec2{0.F, 0.F}},
                Vertex{.pos = Vec3{-0.5F, 0.5F, 0.F}, .color = Vec3{1.0F, 0.0F, 0.0F}, .tex_coord = Vec2{0.F, 1.F}},

                Vertex{.pos = Vec3{0.5F, 0.5F, 0.5F}, .color = Vec3{1.0F, 1.0F, 0.0F}, .tex_coord = Vec2{1.F, 1.F}},
                Vertex{.pos = Vec3{0.5F, -0.5F, 0.5F}, .color = Vec3{0.0F, 1.0F, 1.0F}, .tex_coord = Vec2{1.F, 0.F}},
                Vertex{.pos = Vec3{-0.5F, -0.5F, 0.5F}, .color = Vec3{0.0F, 0.0F, 1.0F}, .tex_coord = Vec2{0.F, 0.F}},
                Vertex{.pos = Vec3{-0.5F, 0.5F, 0.5F}, .color = Vec3{1.0F, 0.0F, 1.0F}, .tex_coord = Vec2{0.F, 1.F}}},
        .indices =
            std::vector<uint16_t>{
                4,
                5,
                6,  //
                6,
                7,
                4,  //
                0,
                1,
                2,  //
                2,
                3,
                0,  //
            },
    };
  }

  vk::BufferCreateInfo create_info(vk::SharingMode sharing_mode) const {
    return vk::BufferCreateInfo{
        // Flags configure sparse buffer memory
        .flags = {},

        // Specifies size of the buffer in bytes
        .size = vertices_size_bytes(),

        .usage = vk::BufferUsageFlagBits::eVertexBuffer,

        // Just like the images in the swap chain, buffers might also be owned by a specific queue family or be shared
        // between multiple at the same time
        .sharingMode = sharing_mode,
    };
  }

  uint32_t vertices_size_bytes() const { return static_cast<uint32_t>(sizeof(Vertex) * vertices.size()); }
  uint32_t indices_size_bytes() const { return static_cast<uint32_t>(sizeof(uint16_t) * indices.size()); }

  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;
};

struct UniformBufferObject {
  using Mat4 = eray::math::Mat4f;

  // A float4x4 matrix must have the same alignment as a float4
  alignas(16) Mat4 model;
  alignas(16) Mat4 view;
  alignas(16) Mat4 proj;
};

class DepthBufferApplication : public vkren::VulkanApplication {
 private:
  vkren::BufferResource vert_buffer_;
  vkren::BufferResource ind_buffer_;

  vkren::ImageResource txt_image_;
  vk::raii::ImageView txt_view_  = nullptr;
  vk::raii::Sampler txt_sampler_ = nullptr;

  void* uniform_buffer_mapped_;
  vkren::BufferResource uniform_buffer_;
  std::vector<vk::DescriptorSet> descriptor_sets_;
  vk::DescriptorSetLayout dsl_;

  vk::raii::PipelineLayout graphics_pipeline_layout_ = nullptr;
  vk::raii::Pipeline graphics_pipeline_              = nullptr;

 public:
  void on_init(vkren::VulkanApplicationContext& ctx) override {
    // == Buffers setup ================================================================================================
    {
      auto vb = VertexBuffer::create();

      auto vertices_region = eray::util::MemoryRegion{vb.vertices.data(), vb.vertices_size_bytes()};
      vert_buffer_         = vkren::BufferResource::create_vertex_buffer(*ctx.device_, vertices_region.size_bytes())
                         .or_panic("Could not create the vertex buffer");
      vert_buffer_.write(vertices_region).or_panic("Could not fill the vertex buffer");

      auto indices_region = eray::util::MemoryRegion{vb.indices.data(), vb.indices_size_bytes()};
      ind_buffer_         = vkren::BufferResource::create_index_buffer(*ctx.device_, indices_region.size_bytes())
                        .or_panic("Could not create a Vertex Buffer");
      ind_buffer_.write(indices_region).or_panic("Could not fill the index buffer");

      // We should have multiple buffers, because multiple frames may be in flight at the same time and
      // we don’t want to update the buffer in preparation of the next frame while a previous one is still
      // reading from it!

      vk::DeviceSize size_bytes = sizeof(UniformBufferObject);
      auto ubo = vkren::BufferResource::create_persistently_mapped_uniform_buffer(*ctx.device_, size_bytes)
                     .or_panic("Could not create the uniform buffer");
      uniform_buffer_ = std::move(ubo.buffer);

      // Copying to uniform buffer each frame means that staging buffer makes no sense.
      uniform_buffer_mapped_ = ubo.mapped_data;
    }

    // == Images setup =================================================================================================
    {
      auto img = eray::res::Image::load_from_path(eray::os::System::executable_dir() / "assets" / "cad.jpeg")
                     .or_panic("cad is not there :(");
      txt_image_ = vkren::ImageResource::create_texture(*ctx.device_, vkren::ImageDescription::from(img))
                       .or_panic("Could not create a texture image");
      txt_image_.upload(img.memory_region()).or_panic("Could not upload the image");
      txt_view_ = txt_image_.create_image_view().or_panic("Could not create the image view");

      auto pdev_props   = ctx.device_->physical_device().getProperties();
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
      txt_sampler_ =
          vkren::Result(ctx.device_->vk().createSampler(sampler_info)).or_panic("Could not create the sampler");
    }

    // == Descriptors setup ============================================================================================
    {
      auto result = vkren::DescriptorSetBuilder::create(ctx.dsl_manager_, ctx.dsl_allocator_)
                        .with_binding(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
                        .with_binding(vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
                        .build_many(kMaxFramesInFlight)
                        .or_panic("Could not create descriptor sets");

      descriptor_sets_ = result.descriptor_sets;
      dsl_             = result.layout;

      auto writer = vkren::DescriptorSetWriter::create(*ctx.device_);
      for (auto i = 0U; i < kMaxFramesInFlight; ++i) {
        writer.write_buffer(0, uniform_buffer_.desc_buffer_info(), vk::DescriptorType::eUniformBuffer);
        writer.write_combined_image_sampler(1, txt_view_, txt_sampler_, vk::ImageLayout::eShaderReadOnlyOptimal);
        writer.write_to_set(descriptor_sets_[i]);
        writer.clear();
      }
    }

    // == Shaders + Graphics Pipeline ==================================================================================
    {
      auto main_binary =
          eray::res::SPIRVShaderBinary::load_from_path(eray::os::System::executable_dir() / "shaders" / "main.spv")
              .or_panic("Could not find main_sh.spv");
      auto main_shader_module =
          vkren::ShaderModule::create(*ctx.device_, main_binary).or_panic("Could not create a main shader module");

      auto binding_desc = Vertex::binding_desc();
      auto attribs_desc = Vertex::attribs_desc();

      auto pipeline = vkren::GraphicsPipelineBuilder::create(*ctx.swap_chain_)
                          .with_shaders(main_shader_module.shader_module, main_shader_module.shader_module)
                          .with_input_state(binding_desc, attribs_desc)
                          .with_descriptor_set_layout(dsl_)
                          .with_depth_test()
                          .with_blending()
                          .build(*ctx.device_)
                          .or_panic("Could not create a graphics pipeline");

      graphics_pipeline_        = std::move(pipeline.pipeline);
      graphics_pipeline_layout_ = std::move(pipeline.layout);
    }
  }

  void on_frame_prepare_sync(vkren::VulkanApplicationContext& ctx, Duration /*delta*/) override {
    auto window_size = ctx.window_->window_size();

    auto t = std::chrono::duration<float>(time()).count();
    auto s = std::sin(t * 0.7F);
    s      = (s * s - 0.5F) * 90.F;
    UniformBufferObject ubo{};
    ubo.model = eray::math::rotation_axis(eray::math::radians(s), eray::math::Vec3f(0.F, 1.F, 0.F));
    ubo.view  = eray::math::translation(eray::math::Vec3f(0.F, 0.F, -4.F));
    ubo.proj  = eray::math::perspective_vk_rh(
        eray::math::radians(80.0F), static_cast<float>(window_size.width) / static_cast<float>(window_size.height),
        0.01F, 10.F);

    memcpy(uniform_buffer_mapped_, &ubo, sizeof(ubo));
  }

  void on_render_begin(vkren::VulkanApplicationContext& /*ctx*/, Duration /*delta*/) override {
    mark_frame_data_dirty();
  }

  void on_record_graphics(vkren::VulkanApplicationContext& ctx, vk::raii::CommandBuffer& graphics_command_buffer,
                          uint32_t image_index) override {
    // We can specify type of the pipeline
    graphics_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    graphics_command_buffer.bindVertexBuffers(0, vert_buffer_.vk_buffer(), {0});
    graphics_command_buffer.bindIndexBuffer(ind_buffer_.vk_buffer(), 0, vk::IndexType::eUint16);

    // Describes the region of framebuffer that the output will be rendered to
    graphics_command_buffer.setViewport(
        0, vk::Viewport{
               .x      = 0.0F,
               .y      = 0.0F,
               .width  = static_cast<float>(ctx.swap_chain_->extent().width),
               .height = static_cast<float>(ctx.swap_chain_->extent().height),
               // Note: min and max depth must be between [0.0F, 1.0F] and min might be higher than max.
               .minDepth = 0.0F,
               .maxDepth = 1.0F  //
           });

    // Unlike vertex and index buffers, descriptor sets are not unique to graphics pipelines. Therefore, we
    // need to specify if we want to bind descriptor sets to the graphics or compute pipeline. The next
    // parameter is the layout that the descriptors are based on. The next three parameters specify the index
    // of the first descriptor set, the number of sets to bind and the array of sets to bind.
    graphics_command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphics_pipeline_layout_, 0,
                                               descriptor_sets_[image_index], nullptr);

    graphics_command_buffer.drawIndexed(12, 1, 0, 0, 0);
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
  auto app = eray::vkren::VulkanApplication::create<DepthBufferApplication>();
  app.run();

  // == Cleanup ========================================================================================================
  eray::os::System::instance().terminate();

  return 0;
}
