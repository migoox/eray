#pragma once

#include <liberay/vkren/common.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

enum class ImageAttachmentType : uint8_t {
  Color,
  DepthStencil,
  Depth,
  Stencil,
};

struct RenderPassAttachmentHandle {
  uint32_t index;
  ImageAttachmentType type;
};

struct RenderPassHandle {
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

struct RenderPass {
  vk::Extent2D extent;
  std::vector<RenderPassAttachmentDependency> attachment_dependencies;
  std::vector<RenderPassAttachmentImageInfo> color_attachments;
  std::optional<RenderPassAttachmentImageInfo> depth_stencil_attachment             = std::nullopt;
  std::optional<RenderPassAttachmentImageInfo> depth_attachment                     = std::nullopt;
  std::optional<RenderPassAttachmentImageInfo> stencil_attachment                   = std::nullopt;
  std::function<void(Device& device, vk::CommandBuffer& cmd_buff)> on_cmd_emit_func = [](auto&, auto&) {};
};

class RenderPassBuilder {
 public:
  RenderPassBuilder& with_dependency(RenderPassAttachmentHandle handle,
                                     vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::AccessFlagBits2 access_mask    = vk::AccessFlagBits2::eShaderRead,
                                     vk::ImageLayout layout             = vk::ImageLayout::eReadOnlyOptimal);
  RenderPassBuilder& with_color_attachment(RenderPassAttachmentHandle handle,
                                           vk::AttachmentLoadOp load_op   = vk::AttachmentLoadOp::eClear,
                                           vk::AttachmentStoreOp store_op = vk::AttachmentStoreOp::eStore);
  RenderPassBuilder& with_msaa_color_attachment(RenderPassAttachmentHandle image_handle,
                                                RenderPassAttachmentHandle resolve_image_handle,
                                                vk::SampleCountFlags samples,
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
  RenderPassBuilder& on_emit(const std::function<void(Device& device, vk::CommandBuffer& cmd_buff)>& emit_func);

  /**
   * @brief Builds the render pass.
   *
   * @param width
   * @param height
   * @return Result<RenderPass, Error>
   * @warning After build is invoked it returns to default state (it does not preserve the current state).
   */
  Result<RenderPass, Error> build(uint32_t width, uint32_t height);  // TODO(migoox): handle resize

 private:
  RenderPass render_pass_;
};

struct RenderPassAttachmentImage {
  vk::raii::ImageView view;
  ImageResource img;
  vk::PipelineStageFlags2 src_stage_mask         = vk::PipelineStageFlagBits2::eTopOfPipe;
  vk::AccessFlags2 src_access_mask               = {};  // NOLINT
  vk::ImageLayout src_layout                     = vk::ImageLayout::eUndefined;
  vk::ClearColorValue clear_color                = vk::ClearColorValue{0.F, 0.F, 0.F, 0.F};
  vk::ClearDepthStencilValue clear_depth_stencil = vk::ClearDepthStencilValue{.depth = 0.F, .stencil = 0U};
};

class RenderGraph;

class RenderGraphBuilder {
 public:
  RenderPassAttachmentHandle with_color_attachment(ImageResource&& color_attachment);
  RenderPassAttachmentHandle with_depth_stencil_attachment(ImageResource&& depth_stencil_attachment);
  RenderPassAttachmentHandle with_depth_attachment(ImageResource&& depth_attachment);
  RenderPassAttachmentHandle with_stencil_attachment(ImageResource&& stencil_attachment);
  Result<RenderPassHandle, Error> with_render_pass(RenderPass&& render_pass);

  Result<RenderGraph, Error> build();

 private:
  std::vector<ImageResource> color_attachments_;
  std::vector<ImageResource> depth_stencil_attachments_;
  std::vector<ImageResource> depth_attachments_;
  std::vector<ImageResource> stencil_attachments_;

  std::vector<RenderPass> render_passes_;
};

class RenderGraph {
 public:
  RenderGraph()                                  = delete;
  RenderGraph(const RenderGraph&)                = delete;
  RenderGraph(RenderGraph&&) noexcept            = default;
  RenderGraph& operator=(const RenderGraph&)     = delete;
  RenderGraph& operator=(RenderGraph&&) noexcept = default;

  void emit(Device& device, vk::CommandBuffer& cmd_buff);
  const RenderPassAttachmentImage& attachment(RenderPassAttachmentHandle handle) const;

 private:
  friend RenderGraphBuilder;

  RenderGraph(std::vector<RenderPassAttachmentImage>&& color_attachments,
              std::vector<RenderPassAttachmentImage>&& depth_stencil_attachments,
              std::vector<RenderPassAttachmentImage>&& depth_attachments,
              std::vector<RenderPassAttachmentImage>&& stencil_attachments, std::vector<RenderPass>&& render_passes)
      : color_attachments_(std::move(color_attachments)),
        depth_stencil_attachments_(std::move(depth_stencil_attachments)),
        depth_attachments_(std::move(depth_attachments)),
        stencil_attachments_(std::move(stencil_attachments)),
        render_passes_(std::move(render_passes)) {}

  void for_each_attachment(const std::function<void(RenderPassAttachmentImage& attachment_image)>& action);
  void for_each_depth_or_stencil(const std::function<void(RenderPassAttachmentImage& attachment_image)>& action);
  RenderPassAttachmentImage& attachment(RenderPassAttachmentHandle handle);

 private:
  std::vector<RenderPassAttachmentImage> color_attachments_;
  std::vector<RenderPassAttachmentImage> depth_stencil_attachments_;
  std::vector<RenderPassAttachmentImage> depth_attachments_;
  std::vector<RenderPassAttachmentImage> stencil_attachments_;

  std::vector<RenderPass> render_passes_;
};

}  // namespace eray::vkren
