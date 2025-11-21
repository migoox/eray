#include <algorithm>
#include <cassert>
#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/render_graph.hpp>
#include <optional>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

RenderPassBuilder& RenderPassBuilder::with_dependency(RenderPassAttachmentHandle handle,
                                                      vk::PipelineStageFlags2 stage_mask,
                                                      vk::AccessFlagBits2 access_mask, vk::ImageLayout layout) {
  render_pass_.attachment_dependencies.emplace_back(RenderPassAttachmentDependency{
      .handle      = handle,
      .stage_mask  = stage_mask,
      .access_mask = access_mask,
      .layout      = layout,
  });
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_color_attachment(RenderPassAttachmentHandle handle,
                                                            vk::AttachmentLoadOp load_op,
                                                            vk::AttachmentStoreOp store_op) {
  render_pass_.color_attachments.emplace_back(RenderPassAttachmentImageInfo{
      .handle         = handle,
      .resolve_handle = std::nullopt,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = vk::SampleCountFlagBits::e1,
  });
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_msaa_color_attachment(RenderPassAttachmentHandle image_handle,
                                                                 RenderPassAttachmentHandle resolve_image_handle,
                                                                 vk::SampleCountFlags samples,
                                                                 vk::AttachmentLoadOp load_op,
                                                                 vk::AttachmentStoreOp store_op) {
  render_pass_.depth_stencil_attachment = RenderPassAttachmentImageInfo{
      .handle         = image_handle,
      .resolve_handle = resolve_image_handle,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = samples,
  };
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_depth_stencil_attachment(RenderPassAttachmentHandle handle,
                                                                    vk::AttachmentLoadOp load_op,
                                                                    vk::AttachmentStoreOp store_op) {
  render_pass_.depth_stencil_attachment = RenderPassAttachmentImageInfo{
      .handle         = handle,
      .resolve_handle = std::nullopt,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = vk::SampleCountFlagBits::e1,
  };
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_depth_attachment(RenderPassAttachmentHandle handle,
                                                            vk::AttachmentLoadOp load_op,
                                                            vk::AttachmentStoreOp store_op) {
  render_pass_.depth_attachment = RenderPassAttachmentImageInfo{
      .handle         = handle,
      .resolve_handle = std::nullopt,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = vk::SampleCountFlagBits::e1,
  };
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_stencil_attachment(RenderPassAttachmentHandle handle,
                                                              vk::AttachmentLoadOp load_op,
                                                              vk::AttachmentStoreOp store_op) {
  render_pass_.stencil_attachment = RenderPassAttachmentImageInfo{
      .handle         = handle,
      .resolve_handle = std::nullopt,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = vk::SampleCountFlagBits::e1,
  };
  return *this;
}

RenderPassBuilder& RenderPassBuilder::on_emit(
    const std::function<void(Device& device, vk::CommandBuffer& cmd_buff)>& emit_func) {
  render_pass_.on_cmd_emit_func = emit_func;
  return *this;
}

Result<RenderPass, Error> RenderPassBuilder::build(uint32_t width, uint32_t height) {
  auto is_valid =
      (!render_pass_.depth_attachment || render_pass_.depth_attachment->handle.type == ImageAttachmentType::Depth) &&
      (!render_pass_.stencil_attachment ||
       render_pass_.stencil_attachment->handle.type == ImageAttachmentType::Stencil) &&
      (!render_pass_.depth_stencil_attachment ||
       render_pass_.depth_stencil_attachment->handle.type == ImageAttachmentType::DepthStencil) &&
      std::ranges::all_of(render_pass_.color_attachments,
                          [](auto& c) { return c.handle.type == ImageAttachmentType::Color; }) &&
      (!render_pass_.depth_attachment || !render_pass_.depth_stencil_attachment) &&
      (!render_pass_.stencil_attachment || !render_pass_.depth_stencil_attachment);

  if (!is_valid) {
    util::Logger::err("Could not emplace a render pass. Attachment handle type does not match the expected type.");
    return std::unexpected(Error{
        .msg  = "Attachment handle type does not match the expected type",
        .code = ErrorCode::InvalidRenderPass{},
    });
  }

  bool loop = std::ranges::any_of(render_pass_.attachment_dependencies, [this](auto& d) {
    return std::ranges::any_of(render_pass_.color_attachments,
                               [&d](const auto& a) { return a.handle.index == d.handle.index; }) ||
           (render_pass_.depth_attachment && render_pass_.depth_attachment->handle.index == d.handle.index) ||
           (render_pass_.stencil_attachment && render_pass_.stencil_attachment->handle.index == d.handle.index) ||
           (render_pass_.depth_stencil_attachment &&
            render_pass_.depth_stencil_attachment->handle.index == d.handle.index);
  });

  if (loop) {
    util::Logger::err(
        "Could not emplace a render pass. One of the provided dependencies has been already provided as an "
        "attachment.");
    return std::unexpected(Error{
        .msg  = "One of the provided dependencies has been already provided as an "
                "attachment.",
        .code = ErrorCode::InvalidRenderPass{},
    });
  }

  render_pass_.extent.width  = width;
  render_pass_.extent.height = height;

  auto res     = std::move(render_pass_);
  render_pass_ = RenderPass{};

  return res;
}

RenderPassAttachmentHandle RenderGraphBuilder::with_color_attachment(ImageResource&& color_attachment) {
  color_attachments_.emplace_back(std::move(color_attachment));
  return RenderPassAttachmentHandle{
      .index = static_cast<uint32_t>(color_attachments_.size() - 1),
      .type  = ImageAttachmentType::Color,
  };
}

RenderPassAttachmentHandle RenderGraphBuilder::with_depth_stencil_attachment(ImageResource&& depth_stencil_attachment) {
  depth_stencil_attachments_.emplace_back(std::move(depth_stencil_attachment));
  return RenderPassAttachmentHandle{
      .index = static_cast<uint32_t>(depth_stencil_attachments_.size() - 1),
      .type  = ImageAttachmentType::DepthStencil,
  };
}

RenderPassAttachmentHandle RenderGraphBuilder::with_depth_attachment(ImageResource&& depth_attachment) {
  depth_attachments_.emplace_back(std::move(depth_attachment));
  return RenderPassAttachmentHandle{
      .index = static_cast<uint32_t>(depth_attachments_.size() - 1),
      .type  = ImageAttachmentType::Depth,
  };
}

RenderPassAttachmentHandle RenderGraphBuilder::with_stencil_attachment(ImageResource&& stencil_attachment) {
  stencil_attachments_.emplace_back(std::move(stencil_attachment));
  return RenderPassAttachmentHandle{
      .index = static_cast<uint32_t>(stencil_attachments_.size() - 1),
      .type  = ImageAttachmentType::Stencil,
  };
}

Result<RenderPassHandle, Error> RenderGraphBuilder::with_render_pass(RenderPass&& render_pass) {
  // It's impossible to create dependency cycles or provide incorrect ordering, because during render pass creation
  // client can only refer to already emplaced render passes (via handles).
  render_passes_.emplace_back(std::move(render_pass));

  auto exists =
      (!render_pass.depth_attachment || render_pass.depth_attachment->handle.index < depth_attachments_.size()) &&
      (!render_pass.stencil_attachment || render_pass.stencil_attachment->handle.index < stencil_attachments_.size()) &&
      (!render_pass.depth_stencil_attachment ||
       render_pass.depth_stencil_attachment->handle.index < depth_stencil_attachments_.size()) &&
      std::ranges::all_of(render_pass.color_attachments,
                          [this](auto& c) { return c.handle.index < color_attachments_.size(); });

  if (!exists) {
    util::Logger::err("Could not emplace a render pass. Attachment is not registered in the builder.");
    return std::unexpected(Error{
        .msg  = "Attachment is not registered in the builder",
        .code = ErrorCode::InvalidRenderPass{},
    });
  }

  return RenderPassHandle{static_cast<uint32_t>(render_passes_.size() - 1)};
}

Result<RenderGraph, Error> RenderGraphBuilder::build() {
  const auto create_attachments = [](std::vector<ImageResource>& src,
                                     std::vector<RenderPassAttachmentImage>& dst) -> Result<void, Error> {
    dst.reserve(src.size());
    for (auto&& a : src) {
      auto img_view = a.create_image_view();
      if (!img_view) {
        return std::unexpected(img_view.error());
      }

      dst.emplace_back(RenderPassAttachmentImage{
          .view = std::move(*img_view),
          .img  = std::move(a),
      });
    }

    return {};
  };

  std::vector<RenderPassAttachmentImage> color_attachments;
  std::vector<RenderPassAttachmentImage> depth_stencil_attachments;
  std::vector<RenderPassAttachmentImage> depth_attachments;
  std::vector<RenderPassAttachmentImage> stencil_attachments;

  if (auto res = create_attachments(color_attachments_, color_attachments); !res) {
    return std::unexpected(res.error());
  }
  if (auto res = create_attachments(depth_attachments_, depth_attachments); !res) {
    return std::unexpected(res.error());
  }
  if (auto res = create_attachments(stencil_attachments_, stencil_attachments); !res) {
    return std::unexpected(res.error());
  }
  if (auto res = create_attachments(depth_stencil_attachments_, depth_stencil_attachments); !res) {
    return std::unexpected(res.error());
  }

  return RenderGraph(std::move(color_attachments), std::move(depth_stencil_attachments), std::move(depth_attachments),
                     std::move(stencil_attachments), std::move(render_passes_));
}

void RenderGraph::for_each_attachment(const std::function<void(RenderPassAttachmentImage& attachment_image)>& action) {
  for (auto& img_info : color_attachments_) {
    action(img_info);
  }
  for (auto& img_info : depth_attachments_) {
    action(img_info);
  }
  for (auto& img_info : stencil_attachments_) {
    action(img_info);
  }
  for (auto& img_info : depth_stencil_attachments_) {
    action(img_info);
  }
}

void RenderGraph::for_each_depth_or_stencil(
    const std::function<void(RenderPassAttachmentImage& attachment_image)>& action) {
  for (auto& img_info : depth_attachments_) {
    action(img_info);
  }
  for (auto& img_info : stencil_attachments_) {
    action(img_info);
  }
  for (auto& img_info : depth_stencil_attachments_) {
    action(img_info);
  }
}

const RenderPassAttachmentImage& RenderGraph::attachment(RenderPassAttachmentHandle handle) const {
  switch (handle.type) {
    case eray::vkren::ImageAttachmentType::Color:
      return color_attachments_[handle.index];
    case eray::vkren::ImageAttachmentType::Depth:
      return depth_attachments_[handle.index];
    case eray::vkren::ImageAttachmentType::Stencil:
      return stencil_attachments_[handle.index];
    case eray::vkren::ImageAttachmentType::DepthStencil:
      return depth_stencil_attachments_[handle.index];
  };
}

RenderPassAttachmentImage& RenderGraph::attachment(RenderPassAttachmentHandle handle) {
  switch (handle.type) {
    case eray::vkren::ImageAttachmentType::Color:
      return color_attachments_[handle.index];
    case eray::vkren::ImageAttachmentType::Depth:
      return depth_attachments_[handle.index];
    case eray::vkren::ImageAttachmentType::Stencil:
      return stencil_attachments_[handle.index];
    case eray::vkren::ImageAttachmentType::DepthStencil:
      return depth_stencil_attachments_[handle.index];
  };
}

void RenderGraph::emit(Device& device, vk::CommandBuffer& cmd_buff) {
  auto color_attachment_infos = std::vector<vk::RenderingAttachmentInfo>();

  std::vector<vk::ImageMemoryBarrier2> image_memory_barriers;

  for_each_attachment([](RenderPassAttachmentImage& img_info) {
    img_info.src_access_mask = {};
    img_info.src_stage_mask  = vk::PipelineStageFlagBits2::eTopOfPipe;
    img_info.src_layout      = vk::ImageLayout::eUndefined;
  });

  for (auto& rp : render_passes_) {
    image_memory_barriers.clear();

    // == Setup the barriers for the dependency images =================================================================
    for (auto dep : rp.attachment_dependencies) {
      auto& img_info = attachment(dep.handle);

      auto barrier = vk::ImageMemoryBarrier2{
          .srcStageMask        = img_info.src_stage_mask,
          .srcAccessMask       = img_info.src_access_mask,
          .dstStageMask        = dep.stage_mask,
          .dstAccessMask       = dep.access_mask,
          .oldLayout           = img_info.src_layout,
          .newLayout           = dep.layout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image               = img_info.img.vk_image(),  //
          .subresourceRange    = img_info.img.full_resource_range(),
      };
      img_info.src_stage_mask  = dep.stage_mask;
      img_info.src_access_mask = dep.access_mask;
      img_info.src_layout      = dep.layout;

      image_memory_barriers.emplace_back(std::move(barrier));
    }

    // == Setup the target image barriers and attachments ==============================================================
    color_attachment_infos.clear();
    color_attachment_infos.reserve(rp.color_attachments.size());
    for (auto c : rp.color_attachments) {
      auto& color_img_info = color_attachments_[c.handle.index];

      auto info = vk::RenderingAttachmentInfo{
          .imageView   = vk::ImageView{color_img_info.view},
          .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
          .loadOp      = c.load_op,
          .storeOp     = c.store_op,
          .clearValue  = color_img_info.clear_color,
      };

      {
        auto dst_stage_mask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        auto dst_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite;
        auto dst_layout      = vk::ImageLayout::eColorAttachmentOptimal;
        auto barrier         = vk::ImageMemoryBarrier2{
                    .srcStageMask        = color_img_info.src_stage_mask,
                    .srcAccessMask       = color_img_info.src_access_mask,
                    .dstStageMask        = dst_stage_mask,
                    .dstAccessMask       = dst_access_mask,
                    .oldLayout           = color_img_info.src_layout,
                    .newLayout           = dst_layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image               = color_attachments_[c.handle.index].img.vk_image(),
                    .subresourceRange    = color_attachments_[c.handle.index].img.full_resource_range(),
        };
        color_img_info.src_stage_mask  = dst_stage_mask;
        color_img_info.src_access_mask = dst_access_mask;
        color_img_info.src_layout      = dst_layout;
        image_memory_barriers.emplace_back(std::move(barrier));
      }

      if (c.resolve_handle) {
        // MSAA is enabled

        auto& resolve_color_img_info = color_attachments_[c.resolve_handle->index];
        info.resolveMode             = vk::ResolveModeFlagBits::eAverage;
        info.resolveImageView        = resolve_color_img_info.view;
        info.resolveImageLayout      = vk::ImageLayout::eColorAttachmentOptimal;

        auto dst_stage_mask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        auto dst_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead;
        auto dst_layout      = vk::ImageLayout::eColorAttachmentOptimal;

        auto resolve_barrier = vk::ImageMemoryBarrier2{
            .srcStageMask        = resolve_color_img_info.src_stage_mask,
            .srcAccessMask       = resolve_color_img_info.src_access_mask,
            .dstStageMask        = dst_stage_mask,
            .dstAccessMask       = dst_access_mask,
            .oldLayout           = resolve_color_img_info.src_layout,
            .newLayout           = dst_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = resolve_color_img_info.img.vk_image(),
            .subresourceRange    = resolve_color_img_info.img.full_resource_range(),
        };
        resolve_color_img_info.src_stage_mask  = dst_stage_mask;
        resolve_color_img_info.src_access_mask = dst_access_mask;
        resolve_color_img_info.src_layout      = dst_layout;
        image_memory_barriers.emplace_back(std::move(resolve_barrier));
      }

      color_attachment_infos.emplace_back(std::move(info));
    }

    std::optional<vk::RenderingAttachmentInfo> depth_info   = std::nullopt;
    std::optional<vk::RenderingAttachmentInfo> stencil_info = std::nullopt;

    if (rp.depth_attachment) {
      const auto& a  = rp.depth_attachment;
      auto& img_info = attachment(a->handle);

      depth_info = vk::RenderingAttachmentInfo{
          .imageView   = vk::ImageView{depth_attachments_[a->handle.index].view},
          .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
          .loadOp      = a->load_op,
          .storeOp     = a->store_op,
          .clearValue  = depth_attachments_[a->handle.index].clear_depth_stencil,
      };

      auto dst_stage_mask =
          vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
      auto dst_access_mask =
          vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
      auto dst_layout = vk::ImageLayout::eDepthAttachmentOptimal;

      auto range       = stencil_attachments_[a->handle.index].img.full_resource_range();
      range.aspectMask = vk::ImageAspectFlagBits::eDepth;

      auto barrier = vk::ImageMemoryBarrier2{
          .srcStageMask        = img_info.src_stage_mask,
          .srcAccessMask       = img_info.src_access_mask,
          .dstStageMask        = dst_stage_mask,
          .dstAccessMask       = dst_access_mask,
          .oldLayout           = img_info.src_layout,
          .newLayout           = dst_layout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image               = depth_attachments_[a->handle.index].img.vk_image(),
          .subresourceRange    = range,
      };
      img_info.src_stage_mask  = dst_stage_mask;
      img_info.src_access_mask = dst_access_mask;
      img_info.src_layout      = dst_layout;
      image_memory_barriers.emplace_back(std::move(barrier));
    }

    if (rp.stencil_attachment) {
      const auto& a  = rp.stencil_attachment;
      auto& img_info = attachment(a->handle);

      stencil_info = vk::RenderingAttachmentInfo{
          .imageView   = vk::ImageView{stencil_attachments_[a->handle.index].view},
          .imageLayout = vk::ImageLayout::eStencilAttachmentOptimal,
          .loadOp      = a->load_op,
          .storeOp     = a->store_op,
          .clearValue  = stencil_attachments_[a->handle.index].clear_depth_stencil,
      };

      auto dst_stage_mask =
          vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
      auto dst_access_mask =
          vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
      auto dst_layout = vk::ImageLayout::eStencilAttachmentOptimal;

      auto range       = stencil_attachments_[a->handle.index].img.full_resource_range();
      range.aspectMask = vk::ImageAspectFlagBits::eStencil;

      auto barrier = vk::ImageMemoryBarrier2{
          .srcStageMask        = vk::PipelineStageFlagBits2::eTopOfPipe,
          .srcAccessMask       = {},
          .dstStageMask        = dst_stage_mask,
          .dstAccessMask       = dst_access_mask,
          .oldLayout           = img_info.src_layout,
          .newLayout           = dst_layout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image               = stencil_attachments_[a->handle.index].img.vk_image(),
          .subresourceRange    = range,
      };
      img_info.src_stage_mask  = dst_stage_mask;
      img_info.src_access_mask = dst_access_mask;
      img_info.src_layout      = dst_layout;
      image_memory_barriers.emplace_back(std::move(barrier));
    }

    if (rp.depth_stencil_attachment) {
      const auto& a  = rp.depth_stencil_attachment;
      auto& img_info = attachment(a->handle);

      depth_info = stencil_info = vk::RenderingAttachmentInfo{
          .imageView   = vk::ImageView{depth_stencil_attachments_[a->handle.index].view},
          .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
          .loadOp      = a->load_op,
          .storeOp     = a->store_op,
          .clearValue  = depth_stencil_attachments_[a->handle.index].clear_color,
      };

      auto dst_stage_mask =
          vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
      auto dst_access_mask =
          vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
      auto dst_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

      auto range       = stencil_attachments_[a->handle.index].img.full_resource_range();
      range.aspectMask = vk::ImageAspectFlagBits::eStencil | vk::ImageAspectFlagBits::eDepth;

      auto barrier = vk::ImageMemoryBarrier2{
          .srcStageMask        = vk::PipelineStageFlagBits2::eTopOfPipe,
          .srcAccessMask       = {},
          .dstStageMask        = dst_stage_mask,
          .dstAccessMask       = dst_access_mask,
          .oldLayout           = img_info.src_layout,
          .newLayout           = dst_layout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image               = stencil_attachments_[a->handle.index].img.vk_image(),
          .subresourceRange    = range,
      };
      img_info.src_stage_mask  = dst_stage_mask;
      img_info.src_access_mask = dst_access_mask;
      img_info.src_layout      = dst_layout;
      image_memory_barriers.emplace_back(std::move(barrier));
    }
    cmd_buff.pipelineBarrier2(vk::DependencyInfo{
        .dependencyFlags         = {},
        .imageMemoryBarrierCount = static_cast<uint32_t>(image_memory_barriers.size()),
        .pImageMemoryBarriers    = image_memory_barriers.data(),
    });
    cmd_buff.beginRendering(vk::RenderingInfo{
        .renderArea =
            vk::Rect2D{
                .offset = {.x = 0, .y = 0},
                .extent = rp.extent,
            },
        .layerCount           = 1,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachment_infos.size()),
        .pColorAttachments    = color_attachment_infos.data(),
        .pDepthAttachment     = depth_info ? &*depth_info : nullptr,
        .pStencilAttachment   = stencil_info ? &*stencil_info : nullptr,
    });
    rp.on_cmd_emit_func(device, cmd_buff);
    cmd_buff.endRendering();
  }
}

}  // namespace eray::vkren
