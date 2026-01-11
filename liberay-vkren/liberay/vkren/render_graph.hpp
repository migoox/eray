#pragma once

#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <type_traits>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

template <typename TEnum, uint32_t TEnumBits>
  requires(std::is_enum_v<TEnum> && (TEnumBits > 0) && (TEnumBits < 32))
struct PackedHandle32 {
  uint32_t _value = 0;

  static constexpr uint32_t kEnumMask = (1U << TEnumBits) - 1U;
  constexpr PackedHandle32()          = default;

  constexpr PackedHandle32(uint32_t index, TEnum tag) : _value((index << TEnumBits) | (uint32_t(tag) & kEnumMask)) {}
  constexpr uint32_t index() const { return _value >> TEnumBits; }
  constexpr TEnum type() const { return TEnum(_value & kEnumMask); }
};

enum class ImageAttachmentType : uint8_t {
  Color,
  DepthStencil,
  Depth,
  Stencil,
};

enum class ShaderStorageType : uint8_t {
  Buffer,
  TexelBuffer,
  Image,
};

using RenderPassAttachmentHandle = PackedHandle32<ImageAttachmentType, 2>;
using ShaderStorageHandle        = PackedHandle32<ShaderStorageType, 2>;

static_assert(sizeof(RenderPassAttachmentHandle) == 4);
static_assert(sizeof(ShaderStorageHandle) == 4);

struct RenderPassHandle {
  uint32_t index;
};

struct ComputePassHandle {
  uint32_t index;
};

struct RenderPassAttachmentImageInfo {
  RenderPassAttachmentHandle handle;
  std::optional<RenderPassAttachmentHandle> resolve_handle;
  vk::AttachmentLoadOp load_op;
  vk::AttachmentStoreOp store_op;
  vk::SampleCountFlags sample_count;
};

struct RenderPassAttachmentDependency {
  RenderPassAttachmentHandle handle;
  vk::PipelineStageFlags2 stage_mask;
  vk::AccessFlagBits2 access_mask;
  vk::ImageLayout layout;
};

struct ShaderStorageDependency {
  ShaderStorageHandle handle;
  vk::PipelineStageFlags2 stage_mask;
  vk::AccessFlagBits2 access_mask = vk::AccessFlagBits2::eShaderStorageRead;
  vk::ImageLayout layout;
};

struct RenderPass {
  vk::Extent2D extent;
  std::vector<RenderPassAttachmentDependency> attachment_dependencies;
  std::vector<ShaderStorageDependency> shader_storage_dependencies;

  std::vector<ShaderStorageHandle> shader_storage;

  std::vector<RenderPassAttachmentImageInfo> color_attachments;
  vk::SampleCountFlagBits samples                                                   = vk::SampleCountFlagBits::e1;
  std::optional<RenderPassAttachmentImageInfo> depth_stencil_attachment             = std::nullopt;
  std::optional<RenderPassAttachmentImageInfo> depth_attachment                     = std::nullopt;
  std::optional<RenderPassAttachmentImageInfo> stencil_attachment                   = std::nullopt;
  std::function<void(Device& device, vk::CommandBuffer& cmd_buff)> on_cmd_emit_func = [](auto&, auto&) {};
};

class RenderGraph;

class RenderPassBuilder {
 public:
  RenderPassBuilder() = delete;
  static RenderPassBuilder create(RenderGraph& render_graph,
                                  vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    auto rpb                 = RenderPassBuilder(&render_graph);
    rpb.render_pass_.samples = sample_count;
    return rpb;
  }

  RenderPassBuilder& with_image_dependency(
      RenderPassAttachmentHandle handle,
      vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2 access_mask    = vk::AccessFlagBits2::eShaderRead,
      vk::ImageLayout layout             = vk::ImageLayout::eReadOnlyOptimal);

  RenderPassBuilder& with_image_dependency(
      ShaderStorageHandle handle, vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2 access_mask = vk::AccessFlagBits2::eShaderStorageRead,
      vk::ImageLayout layout          = vk::ImageLayout::eReadOnlyOptimal);

  RenderPassBuilder& with_buffer_dependency(ShaderStorageHandle handle,
                                            vk::AccessFlagBits2 access_mask = vk::AccessFlagBits2::eShaderStorageRead);

  RenderPassBuilder& with_color_attachment(RenderPassAttachmentHandle handle,
                                           vk::AttachmentLoadOp load_op   = vk::AttachmentLoadOp::eClear,
                                           vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore);
  RenderPassBuilder& with_msaa_color_attachment(RenderPassAttachmentHandle msaa_image_handle,
                                                RenderPassAttachmentHandle resolve_image_handle,
                                                vk::AttachmentLoadOp load_op   = vk::AttachmentLoadOp::eClear,
                                                vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore);
  RenderPassBuilder& with_depth_stencil_attachment(RenderPassAttachmentHandle handle,
                                                   vk::AttachmentLoadOp load_op   = vk::AttachmentLoadOp::eClear,
                                                   vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore);
  RenderPassBuilder& with_depth_attachment(RenderPassAttachmentHandle handle,
                                           vk::AttachmentLoadOp load_op   = vk::AttachmentLoadOp::eClear,
                                           vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore);
  RenderPassBuilder& with_stencil_attachment(RenderPassAttachmentHandle handle,
                                             vk::AttachmentLoadOp load_op   = vk::AttachmentLoadOp::eClear,
                                             vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore);

  RenderPassBuilder& with_shader_storage(ShaderStorageHandle handle);

  RenderPassBuilder& on_emit(const std::function<void(Device& device, vk::CommandBuffer& cmd_buff)>& emit_func);

  /**
   * @brief Builds the render pass.
   *
   * @param width
   * @param height
   * @return Result<RenderPass, Error>
   * @warning After build is invoked it returns to default state (it does not preserve the current state).
   */
  [[nodiscard]] Result<RenderPassHandle, Error> build(uint32_t width, uint32_t height);  // TODO(migoox): handle resize

 private:
  explicit RenderPassBuilder(RenderGraph* render_graph) : render_graph_(render_graph) {}
  RenderPass render_pass_;
  RenderGraph* render_graph_;
};

struct ComputePass {
  std::vector<RenderPassAttachmentDependency> attachment_dependencies;
  std::vector<ShaderStorageDependency> shader_storage_dependencies;

  std::vector<ShaderStorageHandle> shader_storage;

  std::function<void(Device& device, vk::CommandBuffer& cmd_buff)> on_cmd_emit_func = [](auto&, auto&) {};
};

class ComputePassBuilder {
 public:
  ComputePassBuilder() = delete;
  static ComputePassBuilder create(RenderGraph& render_graph) {
    auto cpb = ComputePassBuilder(&render_graph);
    return cpb;
  }

  ComputePassBuilder& with_image_dependency(
      RenderPassAttachmentHandle handle,
      vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2 access_mask    = vk::AccessFlagBits2::eShaderRead,
      vk::ImageLayout layout             = vk::ImageLayout::eReadOnlyOptimal);

  ComputePassBuilder& with_image_dependency(
      ShaderStorageHandle handle, vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2 access_mask = vk::AccessFlagBits2::eShaderStorageRead,
      vk::ImageLayout layout          = vk::ImageLayout::eReadOnlyOptimal);

  ComputePassBuilder& with_buffer_dependency(ShaderStorageHandle handle,
                                             vk::AccessFlagBits2 access_mask = vk::AccessFlagBits2::eShaderStorageRead);

  ComputePassBuilder& with_shader_storage(ShaderStorageHandle handle);

  ComputePassBuilder& on_emit(const std::function<void(Device& device, vk::CommandBuffer& cmd_buff)>& emit_func);

  /**
   * @brief Builds the render pass.
   *
   * @return Result<ComputePass, Error>
   * @warning After build is invoked it returns to default state (it
   * does not preserve the current state).
   */
  [[nodiscard]] Result<ComputePass, Error> build(uint32_t width, uint32_t height);

 private:
  explicit ComputePassBuilder(RenderGraph* render_graph) : render_graph_(render_graph) {}
  ComputePass compute_pass_;
  RenderGraph* render_graph_;
};

struct RenderPassAttachmentImage {
  ImageResource img;
  vk::raii::ImageView view;
  vk::SampleCountFlagBits samples;
  vk::PipelineStageFlags2 src_stage_mask         = vk::PipelineStageFlagBits2::eTopOfPipe;
  vk::AccessFlags2 src_access_mask               = {};  // NOLINT
  vk::ImageLayout src_layout                     = vk::ImageLayout::eUndefined;
  vk::ClearColorValue clear_color                = vk::ClearColorValue{0.F, 0.F, 0.F, 1.F};
  vk::ClearDepthStencilValue clear_depth_stencil = vk::ClearDepthStencilValue{.depth = 1.F, .stencil = 0U};
};

struct ShaderStorageBuffer {
  BufferResource buffer;
  ShaderStorageType type;
  vk::PipelineStageFlags2 src_stage_mask = vk::PipelineStageFlagBits2::eTopOfPipe;
  vk::AccessFlags2 src_access_mask       = {};  // NOLINT
};

struct ShaderStorageImage {
  ImageResource img;
  vk::raii::ImageView view;
  vk::PipelineStageFlags2 src_stage_mask = vk::PipelineStageFlagBits2::eTopOfPipe;
  vk::AccessFlags2 src_access_mask       = {};  // NOLINT
  vk::ImageLayout src_layout             = vk::ImageLayout::eUndefined;
};

/**
 * @brief Render graph allows for rendering dependency graph creation.
 */
class RenderGraph {
 public:
  RenderGraph()                                  = default;
  RenderGraph(const RenderGraph&)                = delete;
  RenderGraph(RenderGraph&&) noexcept            = default;
  RenderGraph& operator=(const RenderGraph&)     = delete;
  RenderGraph& operator=(RenderGraph&&) noexcept = default;

  static RenderGraph create() { return RenderGraph(); }

  RenderPassAttachmentHandle create_color_attachment(Device& device, uint32_t width, uint32_t height,
                                                     bool readable                   = false,
                                                     vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1,
                                                     vk::Format format               = vk::Format::eB8G8R8A8Srgb);

  RenderPassAttachmentHandle create_depth_stencil_attachment(
      Device& device, uint32_t width, uint32_t height, bool readable = false,
      vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1, std::optional<vk::Format> format = std::nullopt);

  RenderPassAttachmentHandle create_depth_attachment(Device& device, uint32_t width, uint32_t height,
                                                     bool readable                    = false,
                                                     vk::SampleCountFlagBits samples  = vk::SampleCountFlagBits::e1,
                                                     std::optional<vk::Format> format = std::nullopt);

  RenderPassAttachmentHandle create_stencil_attachment(Device& device, uint32_t width, uint32_t height,
                                                       bool readable                   = false,
                                                       vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);

  ShaderStorageHandle create_shader_storage_buffer(Device& device, vk::DeviceSize size_bytes);
  ShaderStorageHandle create_shader_storage_texel_buffer(Device& device, vk::DeviceSize size_bytes);
  ShaderStorageHandle create_shader_storage_image(Device& device, const ImageDescription& img_desc,
                                                  vk::ImageAspectFlags image_aspect = vk::ImageAspectFlagBits::eColor);

  RenderPassAttachmentHandle emplace_attachment(ImageResource&& attachment, ImageAttachmentType type);
  RenderPassHandle emplace_render_pass(RenderPass&& render_pass);
  RenderPassHandle emplace_compute_pass(ComputePass&& compute_pass);

  RenderPassBuilder render_pass_builder(vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return RenderPassBuilder::create(*this, sample_count);
  }

  ComputePassBuilder compute_pass_builder() { return ComputePassBuilder::create(*this); }

  void emplace_final_pass_dependency(RenderPassAttachmentHandle handle,
                                     vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::AccessFlagBits2 access_mask    = vk::AccessFlagBits2::eShaderRead,
                                     vk::ImageLayout layout             = vk::ImageLayout::eReadOnlyOptimal);

  void emplace_final_pass_storage_buffer_dependency(
      ShaderStorageHandle handle, vk::AccessFlagBits2 access_mask = vk::AccessFlagBits2::eShaderStorageRead);

  void emplace_final_pass_storage_image_dependency(
      ShaderStorageHandle handle, vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
      vk::AccessFlagBits2 access_mask = vk::AccessFlagBits2::eShaderStorageRead,
      vk::ImageLayout layout          = vk::ImageLayout::eReadOnlyOptimal);

  void emit(Device& device, vk::CommandBuffer& cmd_buff);

  const RenderPassAttachmentImage& attachment(RenderPassAttachmentHandle handle) const;
  RenderPassAttachmentImage& attachment(RenderPassAttachmentHandle handle);

  const RenderPass& render_pass(RenderPassHandle handle) const;

  ShaderStorageBuffer& shader_storage_buffer(ShaderStorageHandle handle);
  const ShaderStorageBuffer& shader_storage_buffer(ShaderStorageHandle handle) const;

  ShaderStorageImage& shader_storage_image(ShaderStorageHandle handle);
  const ShaderStorageImage& shader_storage_image(ShaderStorageHandle handle) const;

  const ComputePass& compute_pass(ComputePassHandle handle) const;

 private:
  friend RenderPassBuilder;

  void for_each_attachment(const std::function<void(RenderPassAttachmentImage& attachment_image)>& action);
  void for_each_shader_storage_buffer(const std::function<void(ShaderStorageBuffer& buffer)>& action);
  void for_each_shader_storage_image(const std::function<void(ShaderStorageImage& buffer)>& action);
  void for_each_depth_or_stencil(const std::function<void(RenderPassAttachmentImage& attachment_image)>& action);

 private:
  std::vector<RenderPassAttachmentImage> color_attachments_;
  std::vector<RenderPassAttachmentImage> depth_stencil_attachments_;
  std::vector<RenderPassAttachmentImage> depth_attachments_;
  std::vector<RenderPassAttachmentImage> stencil_attachments_;

  std::vector<ShaderStorageBuffer> shader_storage_buffers_;
  std::vector<ShaderStorageImage> shader_storage_images_;

  std::vector<RenderPassAttachmentDependency> final_pass_attachments_dependencies_;
  std::vector<ShaderStorageDependency> final_pass_storage_dependencies_;

  std::vector<std::variant<RenderPass, ComputePass>> passes_;
};

}  // namespace eray::vkren
