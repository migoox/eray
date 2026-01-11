#include <algorithm>
#include <cassert>
#include <expected>
#include <liberay/util/logger.hpp>
#include <liberay/util/panic.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/render_graph.hpp>
#include <optional>
#include <variant>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace eray::vkren {

RenderPassBuilder& RenderPassBuilder::with_image_dependency(RenderPassAttachmentHandle handle,
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

RenderPassBuilder& RenderPassBuilder::with_image_dependency(ShaderStorageHandle handle,
                                                            vk::PipelineStageFlags2 stage_mask,
                                                            vk::AccessFlagBits2 access_mask, vk::ImageLayout layout) {
  render_pass_.shader_storage_dependencies.emplace_back(ShaderStorageDependency{
      .handle      = handle,
      .stage_mask  = stage_mask,
      .access_mask = access_mask,
      .layout      = layout,
  });
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_buffer_dependency(ShaderStorageHandle handle,
                                                             vk::AccessFlagBits2 access_mask) {
  render_pass_.shader_storage_dependencies.emplace_back(ShaderStorageDependency{
      .handle      = handle,
      .stage_mask  = vk::PipelineStageFlagBits2::eNone,
      .access_mask = access_mask,
      .layout      = vk::ImageLayout::eUndefined,
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

RenderPassBuilder& RenderPassBuilder::with_msaa_color_attachment(RenderPassAttachmentHandle msaa_image_handle,
                                                                 RenderPassAttachmentHandle resolve_image_handle,
                                                                 vk::AttachmentLoadOp load_op,
                                                                 vk::AttachmentStoreOp store_op) {
  if (render_pass_.samples != render_graph_->attachment(msaa_image_handle).samples) {
    util::panic("Render pass MSAA sample count does not match the color attachment sample count");
  }

  render_pass_.color_attachments.emplace_back(RenderPassAttachmentImageInfo{
      .handle         = msaa_image_handle,
      .resolve_handle = resolve_image_handle,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = render_pass_.samples,
  });
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_depth_stencil_attachment(RenderPassAttachmentHandle handle,
                                                                    vk::AttachmentLoadOp load_op,
                                                                    vk::AttachmentStoreOp store_op) {
  if (render_pass_.samples != render_graph_->attachment(handle).samples) {
    util::panic("Render pass MSAA sample count does not match the color attachment sample count");
  }

  render_pass_.depth_stencil_attachment = RenderPassAttachmentImageInfo{
      .handle         = handle,
      .resolve_handle = std::nullopt,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = render_pass_.samples,
  };
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_depth_attachment(RenderPassAttachmentHandle handle,
                                                            vk::AttachmentLoadOp load_op,
                                                            vk::AttachmentStoreOp store_op) {
  if (render_pass_.samples != render_graph_->attachment(handle).samples) {
    util::panic("Render pass sample count does not match the depth stencil attachment sample count");
  }

  render_pass_.depth_attachment = RenderPassAttachmentImageInfo{
      .handle         = handle,
      .resolve_handle = std::nullopt,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = render_pass_.samples,
  };
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_stencil_attachment(RenderPassAttachmentHandle handle,
                                                              vk::AttachmentLoadOp load_op,
                                                              vk::AttachmentStoreOp store_op) {
  if (render_pass_.samples != render_graph_->attachment(handle).samples) {
    util::panic("Render pass sample count does not match the depth stencil attachment sample count");
  }
  render_pass_.stencil_attachment = RenderPassAttachmentImageInfo{
      .handle         = handle,
      .resolve_handle = std::nullopt,
      .load_op        = load_op,
      .store_op       = store_op,
      .sample_count   = render_pass_.samples,
  };
  return *this;
}

RenderPassBuilder& RenderPassBuilder::with_shader_storage(ShaderStorageHandle handle) {
  render_pass_.shader_storage.push_back(handle);
  return *this;
}

RenderPassBuilder& RenderPassBuilder::on_emit(
    const std::function<void(Device& device, vk::CommandBuffer& cmd_buff)>& emit_func) {
  render_pass_.on_cmd_emit_func = emit_func;
  return *this;
}

Result<RenderPassHandle, Error> RenderPassBuilder::build(uint32_t width, uint32_t height) {
  auto is_valid =
      (!render_pass_.depth_attachment || render_pass_.depth_attachment->handle.type() == ImageAttachmentType::Depth) &&
      (!render_pass_.stencil_attachment ||
       render_pass_.stencil_attachment->handle.type() == ImageAttachmentType::Stencil) &&
      (!render_pass_.depth_stencil_attachment ||
       render_pass_.depth_stencil_attachment->handle.type() == ImageAttachmentType::DepthStencil) &&
      std::ranges::all_of(render_pass_.color_attachments,
                          [](auto& c) { return c.handle.type() == ImageAttachmentType::Color; }) &&
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
                               [&d](const auto& a) { return a.handle.index() == d.handle.index(); }) ||
           (render_pass_.depth_attachment && render_pass_.depth_attachment->handle.index() == d.handle.index()) ||
           (render_pass_.stencil_attachment && render_pass_.stencil_attachment->handle.index() == d.handle.index()) ||
           (render_pass_.depth_stencil_attachment &&
            render_pass_.depth_stencil_attachment->handle.index() == d.handle.index());
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

  auto handle  = render_graph_->emplace_render_pass(std::move(render_pass_));
  render_pass_ = RenderPass{};
  return handle;
}

ComputePassBuilder& ComputePassBuilder::with_image_dependency(RenderPassAttachmentHandle handle,
                                                              vk::PipelineStageFlags2 stage_mask,
                                                              vk::AccessFlagBits2 access_mask, vk::ImageLayout layout) {
  compute_pass_.attachment_dependencies.emplace_back(RenderPassAttachmentDependency{
      .handle      = handle,
      .stage_mask  = stage_mask,
      .access_mask = access_mask,
      .layout      = layout,
  });
  return *this;
}

ComputePassBuilder& ComputePassBuilder::with_image_dependency(ShaderStorageHandle handle,
                                                              vk::PipelineStageFlags2 stage_mask,
                                                              vk::AccessFlagBits2 access_mask, vk::ImageLayout layout) {
  compute_pass_.shader_storage_dependencies.emplace_back(ShaderStorageDependency{
      .handle      = handle,
      .stage_mask  = stage_mask,
      .access_mask = access_mask,
      .layout      = layout,
  });
  return *this;
}

ComputePassBuilder& ComputePassBuilder::with_buffer_dependency(ShaderStorageHandle handle,
                                                               vk::AccessFlagBits2 access_mask) {
  compute_pass_.shader_storage_dependencies.emplace_back(ShaderStorageDependency{
      .handle      = handle,
      .stage_mask  = vk::PipelineStageFlagBits2::eNone,
      .access_mask = access_mask,
      .layout      = vk::ImageLayout::eUndefined,
  });
  return *this;
}

ComputePassBuilder& ComputePassBuilder::with_shader_storage(ShaderStorageHandle handle) {
  compute_pass_.shader_storage.push_back(handle);
  return *this;
}

ComputePassBuilder& ComputePassBuilder::on_emit(
    const std::function<void(Device& device, vk::CommandBuffer& cmd_buff)>& emit_func) {
  compute_pass_.on_cmd_emit_func = emit_func;
  return *this;
}

RenderPassAttachmentHandle RenderGraph::create_color_attachment(Device& device, uint32_t width, uint32_t height,
                                                                bool readable, vk::SampleCountFlagBits samples,
                                                                vk::Format format) {
  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment;
  if (readable) {
    usage |= vk::ImageUsageFlagBits::eSampled;
  } else {
    usage |= vk::ImageUsageFlagBits::eTransientAttachment;
  }
  auto aspect = vk::ImageAspectFlagBits::eColor;

  if (!device.is_format_supported(format, vk::FormatFeatureFlagBits::eColorAttachment)) {
    util::Logger::err("Requested format {} is not supported. Using default format", vk::to_string(format));
  }
  format   = vk::Format::eB8G8R8A8Srgb;
  auto img = ImageResource::create_attachment_image(device, ImageDescription::image2d_desc(format, width, height),
                                                    usage, aspect, samples)
                 .or_panic("Could not create image attachment");

  auto view = img.create_image_view().or_panic("Could not create image view");

  color_attachments_.emplace_back(RenderPassAttachmentImage{
      .img     = std::move(img),
      .view    = std::move(view),
      .samples = samples,
  });

  return RenderPassAttachmentHandle{
      static_cast<uint32_t>(color_attachments_.size() - 1),
      ImageAttachmentType::Color,
  };
}

RenderPassAttachmentHandle RenderGraph::create_depth_stencil_attachment(Device& device, uint32_t width, uint32_t height,
                                                                        bool readable, vk::SampleCountFlagBits samples,
                                                                        std::optional<vk::Format> format) {
  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
  if (readable) {
    usage |= vk::ImageUsageFlagBits::eSampled;
  } else {
    usage |= vk::ImageUsageFlagBits::eTransientAttachment;
  }
  auto aspect = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;

  vk::FormatFeatureFlags features = vk::FormatFeatureFlagBits::eDepthStencilAttachment;

  std::array formats = {
      vk::Format::eD32SfloatS8Uint,  // repeated intentionally
      vk::Format::eD32SfloatS8Uint,
      vk::Format::eD24UnormS8Uint,
  };
  if (format) {
    formats[0] = *format;
  }
  std::optional<vk::Format> final_format = device.get_first_supported_format(formats, features);

  if (!final_format) {
    util::panic("Could not find a supported depth stencil format for this device.");
  }
  if (format && *final_format != *format) {
    util::Logger::err("Requested depth stencil format {} is not supported. Default format will be used",
                      vk::to_string(*format));
  }

  auto img = ImageResource::create_attachment_image(
                 device, ImageDescription::image2d_desc(*final_format, width, height), usage, aspect, samples)
                 .or_panic("Could not create attachment image");
  auto view = img.create_image_view().or_panic("Could not create image view");
  depth_stencil_attachments_.emplace_back(RenderPassAttachmentImage{
      .img     = std::move(img),
      .view    = std::move(view),
      .samples = samples,
  });

  return RenderPassAttachmentHandle{
      static_cast<uint32_t>(depth_stencil_attachments_.size() - 1),
      ImageAttachmentType::DepthStencil,
  };
}

RenderPassAttachmentHandle RenderGraph::create_depth_attachment(Device& device, uint32_t width, uint32_t height,
                                                                bool readable, vk::SampleCountFlagBits samples,
                                                                std::optional<vk::Format> format) {
  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
  if (readable) {
    usage |= vk::ImageUsageFlagBits::eSampled;
  } else {
    usage |= vk::ImageUsageFlagBits::eTransientAttachment;
  }
  auto aspect = vk::ImageAspectFlagBits::eDepth;

  vk::FormatFeatureFlags features = vk::FormatFeatureFlagBits::eDepthStencilAttachment;

  std::array formats = {
      vk::Format::eD32Sfloat,  // repeated intentionally
      vk::Format::eD32Sfloat,
      vk::Format::eD16Unorm,
  };
  if (format) {
    formats[0] = *format;
  }
  std::optional<vk::Format> final_format = device.get_first_supported_format(formats, features);

  if (!final_format) {
    util::panic("Could not find a supported depth stencil format for this device.");
  }
  if (format && *final_format != *format) {
    util::Logger::err("Requested depth stencil format {} is not supported. Default format will be used",
                      vk::to_string(*format));
  }

  auto img = ImageResource::create_attachment_image(
                 device, ImageDescription::image2d_desc(*final_format, width, height), usage, aspect, samples)
                 .or_panic("Could not create attachment image");
  auto view = img.create_image_view().or_panic("Could not create image view");
  depth_attachments_.emplace_back(RenderPassAttachmentImage{
      .img     = std::move(img),
      .view    = std::move(view),
      .samples = samples,
  });

  return RenderPassAttachmentHandle{
      static_cast<uint32_t>(depth_attachments_.size() - 1),
      ImageAttachmentType::Depth,
  };
}

RenderPassAttachmentHandle RenderGraph::create_stencil_attachment(Device& device, uint32_t width, uint32_t height,
                                                                  bool readable, vk::SampleCountFlagBits samples) {
  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
  if (readable) {
    usage |= vk::ImageUsageFlagBits::eSampled;
  } else {
    usage |= vk::ImageUsageFlagBits::eTransientAttachment;
  }
  auto aspect = vk::ImageAspectFlagBits::eStencil;

  ImageResource img =
      ImageResource::create_attachment_image(device, ImageDescription::image2d_desc(vk::Format::eS8Uint, width, height),
                                             usage, aspect, samples)
          .or_panic("Could not create attachment image");
  auto view = img.create_image_view().or_panic("Could not create image view");
  stencil_attachments_.emplace_back(RenderPassAttachmentImage{
      .img     = std::move(img),
      .view    = std::move(view),
      .samples = samples,
  });

  return RenderPassAttachmentHandle{
      static_cast<uint32_t>(stencil_attachments_.size() - 1),
      ImageAttachmentType::Stencil,
  };
}

ShaderStorageHandle RenderGraph::create_shader_storage_buffer(Device& device, vk::DeviceSize size_bytes) {
  auto buffer =
      BufferResource::create_storage_buffer(device, size_bytes).or_panic("Could not create a shader storage buffer");

  shader_storage_buffers_.emplace_back(ShaderStorageBuffer{
      .buffer = std::move(buffer),
      .type   = ShaderStorageType::Buffer,
  });

  return ShaderStorageHandle{
      static_cast<uint32_t>(shader_storage_buffers_.size() - 1),
      ShaderStorageType::Buffer,
  };
}

ShaderStorageHandle RenderGraph::create_shader_storage_texel_buffer(Device& device, vk::DeviceSize size_bytes) {
  auto buffer =
      BufferResource::create_storage_buffer(device, size_bytes).or_panic("Could not create a shader storage buffer");

  shader_storage_buffers_.emplace_back(ShaderStorageBuffer{
      .buffer = std::move(buffer),
      .type   = ShaderStorageType::TexelBuffer,
  });

  return ShaderStorageHandle{
      static_cast<uint32_t>(shader_storage_buffers_.size() - 1),
      ShaderStorageType::TexelBuffer,
  };
}

ShaderStorageHandle RenderGraph::create_shader_storage_image(Device& device, const ImageDescription& img_desc,
                                                             vk::ImageAspectFlags image_aspect) {
  vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;

  auto img = ImageResource::create_attachment_image(device, img_desc, usage, image_aspect, vk::SampleCountFlagBits::e1)
                 .or_panic("Could not create shader storage image");
  auto view = img.create_image_view().or_panic("Could not create image view");

  shader_storage_images_.emplace_back(ShaderStorageImage{
      .img  = std::move(img),
      .view = std::move(view),
  });

  return ShaderStorageHandle{
      static_cast<uint32_t>(shader_storage_images_.size() - 1),
      ShaderStorageType::Image,
  };
}

RenderPassAttachmentHandle RenderGraph::emplace_attachment(ImageResource&& attachment, ImageAttachmentType type) {
  auto view = attachment.create_image_view().or_panic("Could not create image view");
  switch (type) {
    case eray::vkren::ImageAttachmentType::Color:
      color_attachments_.emplace_back(RenderPassAttachmentImage{
          .img     = std::move(attachment),
          .view    = std::move(view),
          .samples = attachment.sample_count,
      });
      return RenderPassAttachmentHandle{
          static_cast<uint32_t>(color_attachments_.size() - 1),
          type,
      };
    case eray::vkren::ImageAttachmentType::Depth:
      depth_attachments_.emplace_back(RenderPassAttachmentImage{
          .img     = std::move(attachment),
          .view    = std::move(view),
          .samples = attachment.sample_count,
      });
      return RenderPassAttachmentHandle{
          static_cast<uint32_t>(depth_attachments_.size() - 1),
          type,
      };
    case eray::vkren::ImageAttachmentType::Stencil:
      stencil_attachments_.emplace_back(RenderPassAttachmentImage{
          .img     = std::move(attachment),
          .view    = std::move(view),
          .samples = attachment.sample_count,
      });
      return RenderPassAttachmentHandle{
          static_cast<uint32_t>(stencil_attachments_.size() - 1),
          type,
      };
    case eray::vkren::ImageAttachmentType::DepthStencil:
      depth_stencil_attachments_.emplace_back(RenderPassAttachmentImage{
          .img     = std::move(attachment),
          .view    = std::move(view),
          .samples = attachment.sample_count,
      });
      return RenderPassAttachmentHandle{
          static_cast<uint32_t>(depth_stencil_attachments_.size() - 1),
          type,
      };
  };
}

RenderPassHandle RenderGraph::emplace_render_pass(RenderPass&& render_pass) {
  // It's impossible to create dependency cycles or provide incorrect ordering, because during render pass creation
  // client can only refer to already emplaced render passes (via handles).
  passes_.emplace_back(std::move(render_pass));

  auto exists =
      (!render_pass.depth_attachment || render_pass.depth_attachment->handle.index() < depth_attachments_.size()) &&
      (!render_pass.stencil_attachment ||
       render_pass.stencil_attachment->handle.index() < stencil_attachments_.size()) &&
      (!render_pass.depth_stencil_attachment ||
       render_pass.depth_stencil_attachment->handle.index() < depth_stencil_attachments_.size()) &&
      std::ranges::all_of(render_pass.color_attachments,
                          [this](auto& c) { return c.handle.index() < color_attachments_.size(); });

  if (!exists) {
    util::panic("Could not emplace a render pass. Attachment is not registered.");
  }

  return RenderPassHandle{.index = static_cast<uint32_t>(passes_.size() - 1)};
}

RenderPassHandle RenderGraph::emplace_compute_pass(ComputePass&& compute_pass) {
  // It's impossible to create dependency cycles or provide incorrect ordering, because during render pass creation
  // client can only refer to already emplaced render passes (via handles).
  passes_.emplace_back(std::move(compute_pass));

  auto exists = std::ranges::all_of(compute_pass.shader_storage, [this](auto& handle) {
    if (handle.type() == ShaderStorageType::Image) {
      return handle.index() < shader_storage_images_.size();
    }
    return handle.index() < shader_storage_buffers_.size();
  });

  if (!exists) {
    util::panic("Could not emplace a compute pass. Shader storage is not registered.");
  }

  return RenderPassHandle{.index = static_cast<uint32_t>(passes_.size() - 1)};
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

void RenderGraph::for_each_shader_storage_buffer(const std::function<void(ShaderStorageBuffer& buffer)>& action) {
  for (auto& b : shader_storage_buffers_) {
    action(b);
  }
}

void RenderGraph::for_each_shader_storage_image(const std::function<void(ShaderStorageImage& buffer)>& action) {
  for (auto& i : shader_storage_images_) {
    action(i);
  }
}

const RenderPassAttachmentImage& RenderGraph::attachment(RenderPassAttachmentHandle handle) const {
  switch (handle.type()) {
    case eray::vkren::ImageAttachmentType::Color:
      return color_attachments_[handle.index()];
    case eray::vkren::ImageAttachmentType::Depth:
      return depth_attachments_[handle.index()];
    case eray::vkren::ImageAttachmentType::Stencil:
      return stencil_attachments_[handle.index()];
    case eray::vkren::ImageAttachmentType::DepthStencil:
      return depth_stencil_attachments_[handle.index()];
  };
}

RenderPassAttachmentImage& RenderGraph::attachment(RenderPassAttachmentHandle handle) {
  switch (handle.type()) {
    case eray::vkren::ImageAttachmentType::Color:
      return color_attachments_[handle.index()];
    case eray::vkren::ImageAttachmentType::Depth:
      return depth_attachments_[handle.index()];
    case eray::vkren::ImageAttachmentType::Stencil:
      return stencil_attachments_[handle.index()];
    case eray::vkren::ImageAttachmentType::DepthStencil:
      return depth_stencil_attachments_[handle.index()];
  };
}

void RenderGraph::emit(Device& device, vk::CommandBuffer& cmd_buff) {
  if (passes_.empty()) {
    return;
  }

  const auto dependency_attachment_image_barrier = [this](std::vector<vk::ImageMemoryBarrier2>& barriers,
                                                          const RenderPassAttachmentDependency& dep) {
    auto& img_info = attachment(dep.handle);
    auto barrier   = vk::ImageMemoryBarrier2{
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

    barriers.emplace_back(std::move(barrier));
  };
  const auto dependency_storage_image_barrier = [this](std::vector<vk::ImageMemoryBarrier2>& barriers,
                                                       const ShaderStorageDependency& dep) {
    if (dep.handle.type() != ShaderStorageType::Image) {
      return;
    }

    auto& img_info = shader_storage_image(dep.handle);
    auto barrier   = vk::ImageMemoryBarrier2{
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

    barriers.emplace_back(std::move(barrier));
  };
  const auto dependency_storage_buffer_barrier = [this](std::vector<vk::BufferMemoryBarrier2>& barriers,
                                                        const ShaderStorageDependency& dep) {
    if (dep.handle.type() == ShaderStorageType::Image) {
      return;
    }

    auto& buffer_info = shader_storage_buffer(dep.handle);
    auto barrier      = vk::BufferMemoryBarrier2{
             .srcStageMask        = buffer_info.src_stage_mask,
             .srcAccessMask       = buffer_info.src_access_mask,
             .dstStageMask        = dep.stage_mask,
             .dstAccessMask       = dep.access_mask,
             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
             .buffer              = buffer_info.buffer.vk_buffer(),  //
             .offset              = 0,
             .size                = buffer_info.buffer.size_bytes,
    };
    buffer_info.src_stage_mask  = dep.stage_mask;
    buffer_info.src_access_mask = dep.access_mask;

    barriers.emplace_back(std::move(barrier));
  };
  const auto target_shader_storage_barriers = [this](std::vector<vk::BufferMemoryBarrier2>& buff_barriers,
                                                     std::vector<vk::ImageMemoryBarrier2>& img_barriers,
                                                     const std::span<const ShaderStorageHandle> handles) {
    auto dst_stage_mask  = vk::PipelineStageFlagBits2::eComputeShader;
    auto dst_access_mask = vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead;

    // https://vulkan.lunarg.com/doc/view/1.4.328.1/windows/antora/guide/latest/storage_image_and_texel_buffers.html#_synchronization_with_storage_images
    auto dst_layout = vk::ImageLayout::eGeneral;

    for (auto handle : handles) {
      if (handle.type() == ShaderStorageType::Image) {
        auto& img_info = shader_storage_images_[handle.index()];
        img_barriers.emplace_back(vk::ImageMemoryBarrier2{
            .srcStageMask        = img_info.src_stage_mask,
            .srcAccessMask       = img_info.src_access_mask,
            .dstStageMask        = dst_stage_mask,
            .dstAccessMask       = dst_access_mask,
            .oldLayout           = img_info.src_layout,
            .newLayout           = dst_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = img_info.img.vk_image(),
            .subresourceRange    = img_info.img.full_resource_range(),
        });
        img_info.src_access_mask = dst_access_mask;
        img_info.src_stage_mask  = dst_stage_mask;
        img_info.src_layout      = dst_layout;
      } else {
        auto& buffer_info = shader_storage_buffers_[handle.index()];
        buff_barriers.emplace_back(vk::BufferMemoryBarrier2{
            .srcStageMask        = buffer_info.src_stage_mask,
            .srcAccessMask       = buffer_info.src_access_mask,
            .dstStageMask        = dst_stage_mask,
            .dstAccessMask       = dst_access_mask,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = buffer_info.buffer.vk_buffer(),
            .offset              = 0,
            .size                = buffer_info.buffer.size_bytes,
        });
        buffer_info.src_access_mask = dst_access_mask;
        buffer_info.src_stage_mask  = dst_stage_mask;
      }
    }
  };

  // https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineStageFlagBits2.html
  //
  // The TOP and BOTTOM pipeline stages are legacy, and applications should prefer VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
  // and VK_PIPELINE_STAGE_2_NONE.

  for_each_attachment([](RenderPassAttachmentImage& img_info) {
    img_info.src_access_mask = vk::AccessFlagBits2::eNone;
    img_info.src_stage_mask  = vk::PipelineStageFlagBits2::eNone;
    img_info.src_layout      = vk::ImageLayout::eUndefined;
  });

  for_each_shader_storage_image([](ShaderStorageImage& img_info) {
    img_info.src_access_mask = vk::AccessFlagBits2::eNone;
    img_info.src_stage_mask  = vk::PipelineStageFlagBits2::eNone;
    img_info.src_layout      = vk::ImageLayout::eUndefined;
  });

  for_each_shader_storage_buffer([](ShaderStorageBuffer& buff) {
    buff.src_access_mask = vk::AccessFlagBits2::eNone;
    buff.src_stage_mask  = vk::PipelineStageFlagBits2::eNone;
  });

  std::vector<vk::RenderingAttachmentInfo> color_attachment_infos;
  std::vector<vk::ImageMemoryBarrier2> image_memory_barriers;
  std::vector<vk::BufferMemoryBarrier2> buffer_memory_barriers;
  for (const auto& pass : passes_) {
    image_memory_barriers.clear();
    buffer_memory_barriers.clear();

    // == Setup barriers for the dependencies (wait for dependencies to become ready) ==================================
    std::visit(
        [&](auto& pass) {
          for (auto& dep : pass.attachment_dependencies) {
            dependency_attachment_image_barrier(image_memory_barriers, dep);
          }
          for (auto& dep : pass.shader_storage_dependencies) {
            if (dep.handle.type() == ShaderStorageType::Image) {
              dependency_storage_image_barrier(image_memory_barriers, dep);
            } else {
              dependency_storage_buffer_barrier(buffer_memory_barriers, dep);
            }
          }
        },
        pass);

    // == Setup the target image barriers and attachments ==============================================================
    if (std::holds_alternative<RenderPass>(pass)) {
      const auto& rp = std::get<RenderPass>(pass);

      color_attachment_infos.clear();
      color_attachment_infos.reserve(rp.color_attachments.size());
      for (auto c : rp.color_attachments) {
        auto& color_img_info = color_attachments_[c.handle.index()];

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
                      .image               = color_attachments_[c.handle.index()].img.vk_image(),
                      .subresourceRange    = color_attachments_[c.handle.index()].img.full_resource_range(),
          };
          color_img_info.src_stage_mask  = dst_stage_mask;
          color_img_info.src_access_mask = dst_access_mask;
          color_img_info.src_layout      = dst_layout;
          image_memory_barriers.emplace_back(std::move(barrier));
        }

        if (c.resolve_handle) {
          // MSAA is enabled

          auto& resolve_color_img_info = color_attachments_[c.resolve_handle->index()];
          info.resolveMode             = vk::ResolveModeFlagBits::eAverage;
          info.resolveImageView        = resolve_color_img_info.view;
          info.resolveImageLayout      = vk::ImageLayout::eColorAttachmentOptimal;

          auto dst_stage_mask  = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
          auto dst_access_mask = vk::AccessFlagBits2::eColorAttachmentWrite;
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
            .imageView   = vk::ImageView{depth_attachments_[a->handle.index()].view},
            .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
            .loadOp      = a->load_op,
            .storeOp     = a->store_op,
            .clearValue  = depth_attachments_[a->handle.index()].clear_depth_stencil,
        };

        auto dst_stage_mask =
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
        auto dst_access_mask =
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
        auto dst_layout = vk::ImageLayout::eDepthAttachmentOptimal;

        auto range       = depth_attachments_[a->handle.index()].img.full_resource_range();
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
            .image               = depth_attachments_[a->handle.index()].img.vk_image(),
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
            .imageView   = vk::ImageView{stencil_attachments_[a->handle.index()].view},
            .imageLayout = vk::ImageLayout::eStencilAttachmentOptimal,
            .loadOp      = a->load_op,
            .storeOp     = a->store_op,
            .clearValue  = stencil_attachments_[a->handle.index()].clear_depth_stencil,
        };

        auto dst_stage_mask =
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
        auto dst_access_mask =
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
        auto dst_layout = vk::ImageLayout::eStencilAttachmentOptimal;

        auto range       = stencil_attachments_[a->handle.index()].img.full_resource_range();
        range.aspectMask = vk::ImageAspectFlagBits::eStencil;

        auto barrier = vk::ImageMemoryBarrier2{
            .srcStageMask        = img_info.src_stage_mask,
            .srcAccessMask       = img_info.src_access_mask,
            .dstStageMask        = dst_stage_mask,
            .dstAccessMask       = dst_access_mask,
            .oldLayout           = img_info.src_layout,
            .newLayout           = dst_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = stencil_attachments_[a->handle.index()].img.vk_image(),
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

        stencil_info = std::nullopt;
        depth_info   = vk::RenderingAttachmentInfo{
              .imageView   = vk::ImageView{depth_stencil_attachments_[a->handle.index()].view},
              .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
              .loadOp      = a->load_op,
              .storeOp     = a->store_op,
              .clearValue  = depth_stencil_attachments_[a->handle.index()].clear_depth_stencil,
        };

        auto dst_stage_mask =
            vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
        auto dst_access_mask =
            vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead;
        auto dst_layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        auto range       = depth_stencil_attachments_[a->handle.index()].img.full_resource_range();
        range.aspectMask = vk::ImageAspectFlagBits::eStencil | vk::ImageAspectFlagBits::eDepth;

        auto barrier = vk::ImageMemoryBarrier2{
            .srcStageMask        = img_info.src_stage_mask,
            .srcAccessMask       = img_info.src_access_mask,
            .dstStageMask        = dst_stage_mask,
            .dstAccessMask       = dst_access_mask,
            .oldLayout           = img_info.src_layout,
            .newLayout           = dst_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = depth_stencil_attachments_[a->handle.index()].img.vk_image(),
            .subresourceRange    = range,
        };
        img_info.src_stage_mask  = dst_stage_mask;
        img_info.src_access_mask = dst_access_mask;
        img_info.src_layout      = dst_layout;
        image_memory_barriers.emplace_back(std::move(barrier));
      }

      target_shader_storage_barriers(buffer_memory_barriers, image_memory_barriers, rp.shader_storage);

      cmd_buff.pipelineBarrier2(vk::DependencyInfo{
          .dependencyFlags          = {},
          .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_memory_barriers.size()),
          .pBufferMemoryBarriers    = buffer_memory_barriers.data(),
          .imageMemoryBarrierCount  = static_cast<uint32_t>(image_memory_barriers.size()),
          .pImageMemoryBarriers     = image_memory_barriers.data(),
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
      cmd_buff.setScissor(0, vk::Rect2D{.offset = vk::Offset2D{.x = 0, .y = 0}, .extent = rp.extent});
      cmd_buff.setViewport(0,
                           vk::Viewport{
                               .x      = 0.0F,
                               .y      = 0.0F,
                               .width  = static_cast<float>(rp.extent.width),
                               .height = static_cast<float>(rp.extent.height),
                               // Note: min and max depth must be between [0.0F, 1.0F] and min might be higher than max.
                               .minDepth = 0.0F,
                               .maxDepth = 1.0F  //
                           });

      rp.on_cmd_emit_func(device, cmd_buff);
      cmd_buff.endRendering();
    } else {
      const auto& cp = std::get<ComputePass>(pass);

      target_shader_storage_barriers(buffer_memory_barriers, image_memory_barriers, cp.shader_storage);
      cmd_buff.pipelineBarrier2(vk::DependencyInfo{
          .dependencyFlags          = {},
          .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_memory_barriers.size()),
          .pBufferMemoryBarriers    = buffer_memory_barriers.data(),
          .imageMemoryBarrierCount  = static_cast<uint32_t>(image_memory_barriers.size()),
          .pImageMemoryBarriers     = image_memory_barriers.data(),
      });
      cp.on_cmd_emit_func(device, cmd_buff);
    }
  }

  // == Setup the barriers for final pass dependencies =================================================================
  image_memory_barriers.clear();
  buffer_memory_barriers.clear();
  for (auto dep : final_pass_attachments_dependencies_) {
    dependency_attachment_image_barrier(image_memory_barriers, dep);
  }
  for (auto dep : final_pass_storage_dependencies_) {
    if (dep.handle.type() == ShaderStorageType::Image) {
      dependency_storage_image_barrier(image_memory_barriers, dep);
    } else {
      dependency_storage_buffer_barrier(buffer_memory_barriers, dep);
    }
  }
  cmd_buff.pipelineBarrier2(vk::DependencyInfo{
      .dependencyFlags          = {},
      .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_memory_barriers.size()),
      .pBufferMemoryBarriers    = buffer_memory_barriers.data(),
      .imageMemoryBarrierCount  = static_cast<uint32_t>(image_memory_barriers.size()),
      .pImageMemoryBarriers     = image_memory_barriers.data(),
  });
}

void RenderGraph::emplace_final_pass_dependency(RenderPassAttachmentHandle handle, vk::PipelineStageFlags2 stage_mask,
                                                vk::AccessFlagBits2 access_mask, vk::ImageLayout layout) {
  final_pass_attachments_dependencies_.emplace_back(RenderPassAttachmentDependency{
      .handle      = handle,
      .stage_mask  = stage_mask,
      .access_mask = access_mask,
      .layout      = layout,
  });
}

void RenderGraph::emplace_final_pass_storage_buffer_dependency(ShaderStorageHandle handle,
                                                               vk::AccessFlagBits2 access_mask) {
  final_pass_storage_dependencies_.emplace_back(ShaderStorageDependency{
      .handle      = handle,
      .stage_mask  = vk::PipelineStageFlagBits2::eNone,
      .access_mask = access_mask,
      .layout      = vk::ImageLayout::eUndefined,
  });
}

void RenderGraph::emplace_final_pass_storage_image_dependency(ShaderStorageHandle handle,
                                                              vk::PipelineStageFlags2 stage_mask,
                                                              vk::AccessFlagBits2 access_mask, vk::ImageLayout layout) {
  final_pass_storage_dependencies_.emplace_back(ShaderStorageDependency{
      .handle      = handle,
      .stage_mask  = stage_mask,
      .access_mask = access_mask,
      .layout      = layout,
  });
}

const RenderPass& RenderGraph::render_pass(RenderPassHandle handle) const {
  return std::get<RenderPass>(passes_[handle.index]);
}

ShaderStorageBuffer& RenderGraph::shader_storage_buffer(ShaderStorageHandle handle) {
  return shader_storage_buffers_[handle.index()];
}
const ShaderStorageBuffer& RenderGraph::shader_storage_buffer(ShaderStorageHandle handle) const {
  return shader_storage_buffers_[handle.index()];
}

ShaderStorageImage& RenderGraph::shader_storage_image(ShaderStorageHandle handle) {
  return shader_storage_images_[handle.index()];
}
const ShaderStorageImage& RenderGraph::shader_storage_image(ShaderStorageHandle handle) const {
  return shader_storage_images_[handle.index()];
}

const ComputePass& RenderGraph::compute_pass(ComputePassHandle handle) const {
  return std::get<ComputePass>(passes_[handle.index]);
}

}  // namespace eray::vkren
