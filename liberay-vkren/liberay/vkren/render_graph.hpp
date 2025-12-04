#pragma once

#include <cstdint>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "liberay/vkren/device.hpp"

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

class RenderGraph;
struct RenderPass;
struct RenderPassAttachmentImage;

class EmitContext {
 public:
  explicit EmitContext(RenderGraph* render_graph, vk::CommandBuffer cmd_buff, Device* device)
      : cmd_buff_(cmd_buff), render_graph_(render_graph), device_(device) {}

  void clear_depth(float d = 1.F);
  void clear_depth_stencil(vk::ClearDepthStencilValue depth_stencil_value);
  void clear_stencil(uint8_t u = 0U);

  /**
   * @brief Clears color image with the provided handle.
   *
   * @param image_handle
   * @param color_value
   */
  void clear_color(RenderPassAttachmentHandle image_handle, vk::ClearColorValue color_value);

  Device& device() { return *device_; }
  vk::CommandBuffer& cmd_buff() { return cmd_buff_; }

 private:
  friend RenderGraph;

  void prepare(RenderPass* render_pass) { render_pass_ = render_pass; }

  void record_in_transfer_dst_layout(
      RenderPassAttachmentImage& img,
      const std::function<void(vk::CommandBuffer& cmd_buff, vk::Image vk_image, vk::ImageLayout dst_layout,
                               vk::ImageSubresourceRange subresource_range)>&);

  vk::CommandBuffer cmd_buff_ = nullptr;
  RenderGraph* render_graph_  = nullptr;
  Device* device_             = nullptr;
  RenderPass* render_pass_    = nullptr;
};

struct RenderPass {
  vk::Extent2D extent;
  std::vector<RenderPassAttachmentDependency> attachment_dependencies;
  std::vector<RenderPassAttachmentImageInfo> color_attachments;
  vk::SampleCountFlagBits samples                                       = vk::SampleCountFlagBits::e1;
  std::optional<RenderPassAttachmentImageInfo> depth_stencil_attachment = std::nullopt;
  std::optional<RenderPassAttachmentImageInfo> depth_attachment         = std::nullopt;
  std::optional<RenderPassAttachmentImageInfo> stencil_attachment       = std::nullopt;
  std::function<void(EmitContext& ctx)> on_cmd_emit_func                = [](auto&) {};
};

class RenderPassBuilder {
 public:
  RenderPassBuilder() = delete;
  static RenderPassBuilder create(RenderGraph& render_graph,
                                  vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    auto rpb                 = RenderPassBuilder(&render_graph);
    rpb.render_pass_.samples = sample_count;
    return rpb;
  }

  RenderPassBuilder& with_dependency(RenderPassAttachmentHandle handle,
                                     vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::AccessFlagBits2 access_mask    = vk::AccessFlagBits2::eShaderRead,
                                     vk::ImageLayout layout             = vk::ImageLayout::eReadOnlyOptimal);
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
  RenderPassBuilder& on_emit(const std::function<void(Device& device, vk::CommandBuffer& cmd_buff)>& emit_func);
  RenderPassBuilder& on_emit(const std::function<void(EmitContext& ctx)>& emit_func);

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

enum class ImageAttachmentUsage : uint8_t {
  FragmentOutputOnly,
  Read,   // for sampled textures
  Write,  // for clearable textures
  ReadWrite,
};

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

  RenderPassAttachmentHandle create_color_attachment(
      Device& device, uint32_t width, uint32_t height,
      ImageAttachmentUsage usage      = ImageAttachmentUsage::FragmentOutputOnly,
      vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1, vk::Format format = vk::Format::eB8G8R8A8Srgb);

  RenderPassAttachmentHandle create_depth_stencil_attachment(
      Device& device, uint32_t width, uint32_t height, bool readable = false,
      vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1, std::optional<vk::Format> format = std::nullopt);

  RenderPassAttachmentHandle create_depth_stencil_attachment(
      Device& device, uint32_t width, uint32_t height,
      ImageAttachmentUsage usage      = ImageAttachmentUsage::FragmentOutputOnly,
      vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1, std::optional<vk::Format> format = std::nullopt);

  RenderPassAttachmentHandle create_depth_attachment(Device& device, uint32_t width, uint32_t height,
                                                     bool readable                    = false,
                                                     vk::SampleCountFlagBits samples  = vk::SampleCountFlagBits::e1,
                                                     std::optional<vk::Format> format = std::nullopt);

  RenderPassAttachmentHandle create_stencil_attachment(Device& device, uint32_t width, uint32_t height,
                                                       bool readable                   = false,
                                                       vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1);

  RenderPassAttachmentHandle emplace_attachment(ImageResource&& attachment, ImageAttachmentType type);
  RenderPassHandle emplace_render_pass(RenderPass&& render_pass);

  RenderPassBuilder render_pass_builder(vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1) {
    return RenderPassBuilder::create(*this, sample_count);
  }

  void emplace_final_pass_dependency(RenderPassAttachmentHandle handle,
                                     vk::PipelineStageFlags2 stage_mask = vk::PipelineStageFlagBits2::eFragmentShader,
                                     vk::AccessFlagBits2 access_mask    = vk::AccessFlagBits2::eShaderRead,
                                     vk::ImageLayout layout             = vk::ImageLayout::eReadOnlyOptimal);

  void emit(Device& device, vk::CommandBuffer& cmd_buff);
  const RenderPassAttachmentImage& attachment(RenderPassAttachmentHandle handle) const;
  RenderPassAttachmentImage& attachment(RenderPassAttachmentHandle handle);
  const RenderPass& render_pass(RenderPassHandle handle) const;

 private:
  friend RenderPassBuilder;

  static void append_usage_flags(vk::ImageUsageFlags& usage_flags, ImageAttachmentUsage usage);
  void for_each_attachment(const std::function<void(RenderPassAttachmentImage& attachment_image)>& action);
  void for_each_depth_or_stencil(const std::function<void(RenderPassAttachmentImage& attachment_image)>& action);

 private:
  friend EmitContext;

  std::vector<RenderPassAttachmentImage> color_attachments_;
  std::vector<RenderPassAttachmentImage> depth_stencil_attachments_;
  std::vector<RenderPassAttachmentImage> depth_attachments_;
  std::vector<RenderPassAttachmentImage> stencil_attachments_;

  std::vector<RenderPassAttachmentDependency> final_pass_dependencies_;

  std::vector<RenderPass> render_passes_;
};

}  // namespace eray::vkren
