#include <liberay/vkren/device.hpp>
#include <liberay/vkren/image_description.hpp>
#include <liberay/vkren/offscreen_renderer.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

Result<OffscreenFragmentRenderer, Error> OffscreenFragmentRenderer::create(Device& device,
                                                                           const ImageDescription& target_image_desc) {
  OffscreenFragmentRenderer off_rend{};
  off_rend._p_device = &device;
  if (auto img_opt = ImageResource::create_attachment_image(device, target_image_desc,
                                                            vk::ImageUsageFlagBits::eColorAttachment |
                                                                vk::ImageUsageFlagBits::eTransferDst |
                                                                vk::ImageUsageFlagBits::eSampled,
                                                            vk::ImageAspectFlagBits::eColor)) {
    off_rend.target_img_ = std::move(*img_opt);
  } else {
    return std::unexpected(img_opt.error());
  }
  off_rend.viewport = vk::Viewport{
      .x        = 0,
      .y        = 0,
      .width    = static_cast<float>(target_image_desc.width),
      .height   = static_cast<float>(target_image_desc.height),
      .minDepth = 0.F,
      .maxDepth = 1.F,
  };

  off_rend.target_img_view_ = Result(off_rend.target_img_.create_image_view()).or_panic("Image view creation failed");

  auto buff = device.begin_single_time_commands();
  off_rend.target_img_.transition_layout(buff, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
  device.end_single_time_commands(buff);

  auto color_attachment_desc = vk::AttachmentDescription{
      .flags          = {},
      .format         = target_image_desc.format,
      .samples        = vk::SampleCountFlagBits::e1,
      .loadOp         = vk::AttachmentLoadOp::eLoad,
      .storeOp        = vk::AttachmentStoreOp::eStore,
      .stencilLoadOp  = vk::AttachmentLoadOp::eDontCare,
      .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
      .initialLayout  = vk::ImageLayout::eColorAttachmentOptimal,
      .finalLayout    = vk::ImageLayout::eColorAttachmentOptimal,
  };

  auto color_ref = vk::AttachmentReference{
      .attachment = 0,
      .layout     = vk::ImageLayout::eColorAttachmentOptimal,
  };

  auto subpass = vk::SubpassDescription{
      .flags                   = {},
      .pipelineBindPoint       = vk::PipelineBindPoint::eGraphics,
      .inputAttachmentCount    = 0,
      .pInputAttachments       = nullptr,
      .colorAttachmentCount    = 1,
      .pColorAttachments       = &color_ref,
      .pResolveAttachments     = nullptr,
      .pDepthStencilAttachment = nullptr,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments    = nullptr,
  };

  auto render_pass_info = vk::RenderPassCreateInfo{
      .flags           = {},
      .attachmentCount = 1,
      .pAttachments    = &color_attachment_desc,
      .subpassCount    = 1,
      .pSubpasses      = &subpass,
      .dependencyCount = 0,
      .pDependencies   = nullptr,
  };

  off_rend.render_pass_ = Result(device->createRenderPass(render_pass_info)).or_panic("Render pass creation failed");

  auto attachments = std::array{*off_rend.target_img_view_};
  auto fb_info     = vk::FramebufferCreateInfo{
          .flags           = {},
          .renderPass      = *off_rend.render_pass_,
          .attachmentCount = static_cast<uint32_t>(attachments.size()),
          .pAttachments    = attachments.data(),
          .width           = target_image_desc.width,
          .height          = target_image_desc.height,
          .layers          = 1,
  };

  off_rend.framebuffer_        = Result(device->createFramebuffer(fb_info)).or_panic("Framebuffer creation failed");
  off_rend.finished_semaphore_ = Result(device->createSemaphore(vk::SemaphoreCreateInfo{}))
                                     .or_panic("Could not create a semaphore for offscreen rendering");

  auto command_pool_info = vk::CommandPoolCreateInfo{
      .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = device.graphics_queue_family(),
  };
  off_rend.cmd_pool_ = Result(device->createCommandPool(command_pool_info)).or_panic("Could not create a command pool");

  auto alloc_info = vk::CommandBufferAllocateInfo{
      .commandPool        = off_rend.cmd_pool_,
      .level              = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  off_rend.cmd_buff_ =
      std::move(Result(device->allocateCommandBuffers(alloc_info)).or_panic("Could not allocate a command buffer")[0]);

  return off_rend;
}

void OffscreenFragmentRenderer::init_pipeline(vk::ShaderModule vertex_module, vk::ShaderModule fragment_module,
                                              vk::DescriptorSetLayout descriptor_set_layout) {
  // --- Shader stages ---
  vk::PipelineShaderStageCreateInfo vert_stage{
      .flags = {}, .stage = vk::ShaderStageFlagBits::eVertex, .module = vertex_module, .pName = "mainVert"};

  vk::PipelineShaderStageCreateInfo frag_stage{
      .flags = {}, .stage = vk::ShaderStageFlagBits::eFragment, .module = fragment_module, .pName = "mainFrag"};

  std::array stages = {vert_stage, frag_stage};

  // --- Fixed-function states ---
  vk::PipelineVertexInputStateCreateInfo vertex_input{.flags                           = {},
                                                      .vertexBindingDescriptionCount   = 0,
                                                      .pVertexBindingDescriptions      = nullptr,
                                                      .vertexAttributeDescriptionCount = 0,
                                                      .pVertexAttributeDescriptions    = nullptr};

  vk::PipelineInputAssemblyStateCreateInfo input_assembly{
      .flags = {}, .topology = vk::PrimitiveTopology::eTriangleList, .primitiveRestartEnable = vk::False};

  vk::Viewport viewport{.x        = 0.0F,
                        .y        = 0.0F,
                        .width    = static_cast<float>(target_img_.description.width),
                        .height   = static_cast<float>(target_img_.description.height),
                        .minDepth = 0.0F,
                        .maxDepth = 1.0F};

  vk::Rect2D scissor{
      .offset = vk::Offset2D{.x = 0, .y = 0},
      .extent = vk::Extent2D{.width = target_img_.description.width, .height = target_img_.description.height}};

  vk::PipelineViewportStateCreateInfo viewport_state{
      .flags = {}, .viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};

  vk::PipelineRasterizationStateCreateInfo raster{.flags                   = {},
                                                  .depthClampEnable        = vk::False,
                                                  .rasterizerDiscardEnable = vk::False,
                                                  .polygonMode             = vk::PolygonMode::eFill,
                                                  .cullMode                = vk::CullModeFlagBits::eNone,
                                                  .frontFace               = vk::FrontFace::eCounterClockwise,
                                                  .depthBiasEnable         = vk::False,
                                                  .depthBiasConstantFactor = 0.0F,
                                                  .depthBiasClamp          = 0.0F,
                                                  .depthBiasSlopeFactor    = 0.0F,
                                                  .lineWidth               = 1.0F};

  vk::PipelineMultisampleStateCreateInfo multisample{.flags                 = {},
                                                     .rasterizationSamples  = vk::SampleCountFlagBits::e1,
                                                     .sampleShadingEnable   = vk::False,
                                                     .minSampleShading      = 1.0F,
                                                     .pSampleMask           = nullptr,
                                                     .alphaToCoverageEnable = vk::False,
                                                     .alphaToOneEnable      = vk::False};

  vk::PipelineColorBlendAttachmentState blend_attachment{
      .blendEnable         = vk::True,
      .srcColorBlendFactor = vk::BlendFactor::eOne,
      .dstColorBlendFactor = vk::BlendFactor::eOne,
      .colorBlendOp        = vk::BlendOp::eMin,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eOne,
      .alphaBlendOp        = vk::BlendOp::eMin,
      .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

  vk::PipelineColorBlendStateCreateInfo blend{
      .flags = {}, .logicOpEnable = vk::False, .attachmentCount = 1, .pAttachments = &blend_attachment};

  vk::PipelineLayoutCreateInfo layout_info{.flags                  = {},
                                           .setLayoutCount         = 1,
                                           .pSetLayouts            = &descriptor_set_layout,
                                           .pushConstantRangeCount = 0,
                                           .pPushConstantRanges    = nullptr};
  pipeline_layout_ = Result((*_p_device)->createPipelineLayout(layout_info))
                         .or_panic("Could not create offscreen renderer pipeline layout");

  vk::GraphicsPipelineCreateInfo pipeline_info{.flags               = {},
                                               .stageCount          = static_cast<uint32_t>(stages.size()),
                                               .pStages             = stages.data(),
                                               .pVertexInputState   = &vertex_input,
                                               .pInputAssemblyState = &input_assembly,
                                               .pTessellationState  = nullptr,
                                               .pViewportState      = &viewport_state,
                                               .pRasterizationState = &raster,
                                               .pMultisampleState   = &multisample,
                                               .pDepthStencilState  = nullptr,
                                               .pColorBlendState    = &blend,
                                               .pDynamicState       = nullptr,
                                               .layout              = *pipeline_layout_,
                                               .renderPass          = *render_pass_,
                                               .subpass             = 0,
                                               .basePipelineHandle  = nullptr,
                                               .basePipelineIndex   = -1};

  pipeline_ = Result((*_p_device)->createGraphicsPipeline(nullptr, pipeline_info))
                  .or_panic("Could not create offscreen renderer pipeline");
}

void OffscreenFragmentRenderer::render_once(vk::DescriptorSet descriptor_set, vk::ClearColorValue clear_color) {
  cmd_buff_.reset();
  cmd_buff_.begin(vk::CommandBufferBeginInfo{});

  // == Change Layout ==================================================================================================
  auto image_barrier = vk::ImageMemoryBarrier2{
      .srcStageMask        = vk::PipelineStageFlagBits2::eFragmentShader,
      .srcAccessMask       = vk::AccessFlagBits2::eShaderRead,
      .dstStageMask        = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      .dstAccessMask       = vk::AccessFlagBits2::eColorAttachmentWrite,
      .oldLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
      .newLayout           = vk::ImageLayout::eColorAttachmentOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image               = target_image(),
      .subresourceRange =
          vk::ImageSubresourceRange{
              .aspectMask     = vk::ImageAspectFlagBits::eColor,
              .baseMipLevel   = 0,
              .levelCount     = 1,
              .baseArrayLayer = 0,
              .layerCount     = 1,
          },
  };

  auto dependency_info = vk::DependencyInfo{
      .dependencyFlags         = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers    = &image_barrier,
  };
  cmd_buff_.pipelineBarrier2(dependency_info);

  // == Render =========================================================================================================
  std::array<vk::ClearValue, 1> clear_values = {clear_color};

  auto rp_begin = vk::RenderPassBeginInfo{
      .renderPass  = *render_pass_,
      .framebuffer = *framebuffer_,
      .renderArea =
          vk::Rect2D{
              .offset = vk::Offset2D{.x = 0, .y = 0},
              .extent =
                  vk::Extent2D{
                      .width  = target_img_.description.width,
                      .height = target_img_.description.height,
                  },
          },
      .clearValueCount = static_cast<uint32_t>(clear_values.size()),
      .pClearValues    = clear_values.data(),
  };

  cmd_buff_.setViewport(0, viewport);
  cmd_buff_.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_, 0, descriptor_set, nullptr);
  cmd_buff_.beginRenderPass(rp_begin, vk::SubpassContents::eInline);
  cmd_buff_.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline_);
  cmd_buff_.draw(3, 1, 0, 0);
  cmd_buff_.endRenderPass();

  // == Change Layout ==================================================================================================
  image_barrier = vk::ImageMemoryBarrier2{
      .srcStageMask        = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      .srcAccessMask       = vk::AccessFlagBits2::eColorAttachmentWrite,
      .dstStageMask        = vk::PipelineStageFlagBits2::eFragmentShader,
      .dstAccessMask       = vk::AccessFlagBits2::eShaderRead,
      .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
      .newLayout           = vk::ImageLayout::eShaderReadOnlyOptimal,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image               = target_image(),
      .subresourceRange =
          vk::ImageSubresourceRange{
              .aspectMask     = vk::ImageAspectFlagBits::eColor,
              .baseMipLevel   = 0,
              .levelCount     = 1,
              .baseArrayLayer = 0,
              .layerCount     = 1,
          },
  };

  dependency_info = vk::DependencyInfo{
      .dependencyFlags         = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers    = &image_barrier,
  };

  cmd_buff_.pipelineBarrier2(dependency_info);
  cmd_buff_.end();

  // == Submit =========================================================================================================
  auto submit_info = vk::SubmitInfo{
      .waitSemaphoreCount   = 0,
      .pWaitSemaphores      = nullptr,
      .pWaitDstStageMask    = nullptr,
      .commandBufferCount   = 1,
      .pCommandBuffers      = &*cmd_buff_,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores    = &*finished_semaphore_,
  };

  _p_device->graphics_queue().submit(submit_info);
}

void OffscreenFragmentRenderer::clear(vk::ClearColorValue clear_value) {
  auto cmd = _p_device->begin_single_time_commands();

  target_img_.transition_layout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eGeneral);

  vk::ImageSubresourceRange range{
      .aspectMask     = vk::ImageAspectFlagBits::eColor,
      .baseMipLevel   = 0,
      .levelCount     = 1,
      .baseArrayLayer = 0,
      .layerCount     = 1,
  };
  cmd.clearColorImage(target_img_.vk_image(), vk::ImageLayout::eGeneral, clear_value, range);

  target_img_.transition_layout(cmd, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal);

  _p_device->end_single_time_commands(cmd);
}

void OffscreenFragmentRenderer::set_viewport(int x, int y, int width, int height) {
  viewport = vk::Viewport{
      .x      = static_cast<float>(x),
      .y      = static_cast<float>(y),
      .width  = static_cast<float>(width),
      .height = static_cast<float>(height),
  };
}

}  // namespace eray::vkren
