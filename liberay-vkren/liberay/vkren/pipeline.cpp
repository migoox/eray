#include <liberay/vkren/error.hpp>
#include <liberay/vkren/pipeline.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

GraphicsPipelineBuilder::GraphicsPipelineBuilder(const SwapChain& swap_chain) {
  // == Dynamic States =================================================================================================
  _dynamic_states = std::vector{
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
  };
  _viewport_state = vk::PipelineViewportStateCreateInfo{.viewportCount = 1, .scissorCount = 1};

  // == Input State ====================================================================================================
  _vertex_input_state.vertexAttributeDescriptionCount = 0;
  _vertex_input_state.vertexBindingDescriptionCount   = 0;

  // == Input Assembly =================================================================================================
  _input_assembly.topology               = vk::PrimitiveTopology::eTriangleList;
  _input_assembly.primitiveRestartEnable = vk::False;

  // == Rasterizer =====================================================================================================
  // TODO(migoox): Add Depth Clamp setter
  _rasterizer.depthClampEnable = vk::False;

  _rasterizer.polygonMode = vk::PolygonMode::eFill;

  // NOTE: The maximum line width that is supported depends on the hardware and any lin thicker
  // than 1.0F requires to enable the wideLines GPU feature.
  _rasterizer.lineWidth = 1.0F;

  _rasterizer.cullMode  = vk::CullModeFlagBits::eBack;
  _rasterizer.frontFace = vk::FrontFace::eClockwise;

  // Polygons that are coplanar in 3D space can be made to appear as if they are not coplanar by adding a z-bias
  // (or depth bias) to each one. This is a technique commonly used to ensure that shadows in a scene are
  // displayed properly. For instance, a shadow on a wall will likely have the same depth value as the wall. If an
  // application renders a wall first and then a shadow, the shadow might not be visible, or depth artifacts might
  // be visible.
  _rasterizer.depthBiasEnable      = vk::False;
  _rasterizer.depthBiasSlopeFactor = 1.0F;

  // == Multisampling ==================================================================================================
  if (swap_chain.msaa_enabled()) {
    _multisampling.sampleShadingEnable  = vk::True;
    _multisampling.rasterizationSamples = swap_chain.msaa_sample_count();
  } else {
    _multisampling.sampleShadingEnable  = vk::False;
    _multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
  }

  // TODO(migoox): Fill the rest of the multisampling fields

  // == Depth and Stencil Testing ======================================================================================
  _depth_stencil.depthTestEnable       = vk::False;
  _depth_stencil.depthWriteEnable      = vk::True;
  _depth_stencil.depthCompareOp        = vk::CompareOp::eLess;
  _depth_stencil.depthBoundsTestEnable = vk::False;
  _depth_stencil.stencilTestEnable     = vk::False;

  // == Color blending =================================================================================================
  _color_blend.blendEnable = vk::False;

  _color_blend.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  _color_blend.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
  _color_blend.colorBlendOp        = vk::BlendOp::eAdd;

  _color_blend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  _color_blend.dstAlphaBlendFactor = vk::BlendFactor::eZero;
  _color_blend.alphaBlendOp        = vk::BlendOp::eAdd;

  _color_blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

  _color_attachment_format = swap_chain.color_attachment_format();
  _depth_stencil_format    = swap_chain.depth_stencil_attachment_format();

  // == Pipeline layouts ===============================================================================================
  _pipeline_layout.setLayoutCount         = 0;
  _pipeline_layout.pushConstantRangeCount = 0;
}

GraphicsPipelineBuilder GraphicsPipelineBuilder::create(const SwapChain& swap_chain) {
  return GraphicsPipelineBuilder(swap_chain);
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_shaders(vk::ShaderModule vertex_shader,
                                                               vk::ShaderModule fragment_shader,
                                                               util::zstring_view vertex_shader_entry_point,
                                                               util::zstring_view fragment_shader_entry_point) {
  _shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
      .stage  = vk::ShaderStageFlagBits::eVertex,
      .module = vertex_shader,
      .pName  = vertex_shader_entry_point.empty() ? kDefaultVertexShaderEntryPoint.c_str()
                                                  : vertex_shader_entry_point.c_str(),
  });
  _shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
      .stage  = vk::ShaderStageFlagBits::eFragment,
      .module = fragment_shader,
      .pName  = fragment_shader_entry_point.empty() ? kDefaultFragmentShaderEntryPoint.c_str()
                                                    : fragment_shader_entry_point.c_str(),
  });

  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_tessellation_stage(
    vk::ShaderModule tess_control_shader, vk::ShaderModule tess_eval_shader, uint32_t patch_control_point_count,
    util::zstring_view tess_control_shader_entry_point, util::zstring_view tess_eval_shader_entry_point) {
  _shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
      .stage  = vk::ShaderStageFlagBits::eTessellationControl,
      .module = tess_control_shader,
      .pName  = tess_control_shader_entry_point.empty() ? kDefaultTessellationControlShaderEntryPoint.c_str()
                                                        : tess_control_shader_entry_point.c_str(),
  });
  _shader_stages.push_back(vk::PipelineShaderStageCreateInfo{
      .stage  = vk::ShaderStageFlagBits::eTessellationEvaluation,
      .module = tess_eval_shader,
      .pName  = tess_eval_shader_entry_point.empty() ? kDefaultTessellationEvalShaderEntryPoint.c_str()
                                                     : tess_eval_shader_entry_point.c_str(),
  });
  tess_stage                     = true;
  _tess_stage.patchControlPoints = patch_control_point_count;

  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_tessellation_domain_origin(
    vk::TessellationDomainOrigin domain_origin) {
  _tess_domain_origin.domainOrigin = domain_origin;
  _tess_stage.pNext                = &_tess_domain_origin;
  return *this;
}

Result<Pipeline, Error> GraphicsPipelineBuilder::build(const Device& device) {
  // == Shader stage ===================================================================================================
  assert(!_shader_stages.empty() && "Shader stages must be provided");

  // == Dynamic states =================================================================================================

  // Most of the pipeline state needs to be baked into the pipeline state. For example changing the size of a
  // viewport, line width and blend constants can be changed dynamically without the full pipeline recreation.
  //
  // Note: This will cause the configuration of these values to be ignored, and you will be able (and required)
  // to specify the data at drawing time.

  auto dynamic_states = std::vector{
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
  };

  auto dynamic_state = vk::PipelineDynamicStateCreateInfo{
      .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),  //
      .pDynamicStates    = dynamic_states.data(),                         //
  };

  // With dynamic state only the count is necessary.

  // == Input assembly =================================================================================================
  // TODO(migoox): Add multiple color attachments support
  vk::PipelineRenderingCreateInfo pipeline_rendering_create_info{
      .colorAttachmentCount    = 1,
      .pColorAttachmentFormats = &_color_attachment_format,
      .depthAttachmentFormat   = _depth_stencil_format,
  };

  // TODO(migoox): Add multiple color attachments support
  auto color_blending_info = vk::PipelineColorBlendStateCreateInfo{
      .logicOpEnable   = vk::False,
      .logicOp         = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments    = &_color_blend,  //
  };

  return device->createPipelineLayout(_pipeline_layout)
      .and_then([&](vk::raii::PipelineLayout&& layout) {
        auto pipeline_info = vk::GraphicsPipelineCreateInfo{
            .pNext               = &pipeline_rendering_create_info,
            .stageCount          = static_cast<uint32_t>(_shader_stages.size()),
            .pStages             = _shader_stages.data(),
            .pVertexInputState   = &_vertex_input_state,
            .pInputAssemblyState = &_input_assembly,
            .pTessellationState  = tess_stage ? &_tess_stage : nullptr,
            .pViewportState      = &_viewport_state,
            .pRasterizationState = &_rasterizer,
            .pMultisampleState   = &_multisampling,
            .pDepthStencilState  = &_depth_stencil,
            .pColorBlendState    = &color_blending_info,
            .pDynamicState       = &dynamic_state,
            .layout              = layout,
            .renderPass          = nullptr,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        };

        return device->createGraphicsPipeline(nullptr, pipeline_info)
            .transform([l = std::move(layout)](vk::raii::Pipeline&& p) mutable {
              return Pipeline{
                  .pipeline = std::move(p),
                  .layout   = std::move(l),
              };
            });
      })
      .transform_error([](auto err) {
        return Error{
            .msg     = "Graphics Pipeline creation failure",
            .code    = ErrorCode::VulkanObjectCreationFailure{},
            .vk_code = err,
        };
      });
}

Result<vk::raii::Pipeline, Error> GraphicsPipelineBuilder::build(const Device& device, vk::PipelineLayout layout) {
  // == Shader stage ===================================================================================================
  assert(!_shader_stages.empty() && "Shader stages must be provided");

  // == Dynamic states =================================================================================================

  // Most of the pipeline state needs to be baked into the pipeline state. For example changing the size of a
  // viewport, line width and blend constants can be changed dynamically without the full pipeline recreation.
  //
  // Note: This will cause the configuration of these values to be ignored, and you will be able (and required)
  // to specify the data at drawing time.

  auto dynamic_states = std::vector{
      vk::DynamicState::eViewport,
      vk::DynamicState::eScissor,
  };

  auto dynamic_state = vk::PipelineDynamicStateCreateInfo{
      .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),  //
      .pDynamicStates    = dynamic_states.data(),                         //
  };

  // With dynamic state only the count is necessary.

  // == Input assembly =================================================================================================
  // TODO(migoox): Add multiple color attachments support
  vk::PipelineRenderingCreateInfo pipeline_rendering_create_info{
      .colorAttachmentCount    = 1,
      .pColorAttachmentFormats = &_color_attachment_format,
      .depthAttachmentFormat   = _depth_stencil_format,
  };

  // TODO(migoox): Add multiple color attachments support
  auto color_blending_info = vk::PipelineColorBlendStateCreateInfo{
      .logicOpEnable   = vk::False,
      .logicOp         = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments    = &_color_blend,  //
  };

  auto pipeline_info = vk::GraphicsPipelineCreateInfo{
      .pNext               = &pipeline_rendering_create_info,
      .stageCount          = static_cast<uint32_t>(_shader_stages.size()),
      .pStages             = _shader_stages.data(),
      .pVertexInputState   = &_vertex_input_state,
      .pInputAssemblyState = &_input_assembly,
      .pTessellationState  = tess_stage ? &_tess_stage : nullptr,
      .pViewportState      = &_viewport_state,
      .pRasterizationState = &_rasterizer,
      .pMultisampleState   = &_multisampling,
      .pDepthStencilState  = &_depth_stencil,
      .pColorBlendState    = &color_blending_info,
      .pDynamicState       = &dynamic_state,
      .layout              = layout,
      .renderPass          = nullptr,
      .basePipelineHandle  = VK_NULL_HANDLE,
      .basePipelineIndex   = -1,
  };

  return device->createGraphicsPipeline(nullptr, pipeline_info).transform_error([](auto err) {
    return Error{
        .msg     = "Graphics Pipeline creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = err,
    };
  });
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_primitive_topology(vk::PrimitiveTopology topology,
                                                                          bool primitive_restart_enable) {
  _input_assembly = vk::PipelineInputAssemblyStateCreateInfo{
      .topology               = topology,
      .primitiveRestartEnable = static_cast<vk::Bool32>(primitive_restart_enable),
  };
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_polygon_mode(vk::PolygonMode polygon_mode, float line_width) {
  _rasterizer.polygonMode = polygon_mode;
  _rasterizer.lineWidth   = line_width;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_cull_mode(vk::CullModeFlags cull_mode,
                                                                 vk::FrontFace front_face) {
  _rasterizer.cullMode  = cull_mode;
  _rasterizer.frontFace = front_face;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_depth_bias(float slope_factor) {
  _rasterizer.depthBiasEnable      = vk::True;
  _rasterizer.depthBiasSlopeFactor = slope_factor;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_multisampling(vk::SampleCountFlagBits rasterization_samples) {
  _multisampling.rasterizationSamples = rasterization_samples;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_sample_shading(float min_sample_shading) {
  _multisampling.sampleShadingEnable = vk::True;
  _multisampling.minSampleShading    = min_sample_shading;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_src_blend_factors(vk::BlendFactor color_blend_factor,
                                                                         vk::BlendFactor alpha_blend_factor) {
  _color_blend.srcColorBlendFactor = color_blend_factor;
  _color_blend.srcAlphaBlendFactor = alpha_blend_factor;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_dst_blend_factors(vk::BlendFactor color_blend_factor,
                                                                         vk::BlendFactor alpha_blend_factor) {
  _color_blend.dstColorBlendFactor = color_blend_factor;
  _color_blend.dstAlphaBlendFactor = alpha_blend_factor;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_blend_ops(vk::BlendOp color_blend_op,
                                                                 vk::BlendOp alpha_blend_op) {
  _color_blend.colorBlendOp = color_blend_op;
  _color_blend.alphaBlendOp = alpha_blend_op;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_blending() {
  _color_blend.blendEnable = vk::True;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_color_write_mask(vk::ColorComponentFlags flags) {
  _color_blend.colorWriteMask = flags;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_depth_test(bool test_write) {
  _depth_stencil.depthTestEnable  = vk::True;
  _depth_stencil.depthWriteEnable = static_cast<vk::Bool32>(test_write);
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_depth_bounds_test(float min_depth_bounds,
                                                                         float max_depth_bounds) {
  _depth_stencil.depthBoundsTestEnable = vk::True;
  _depth_stencil.minDepthBounds        = min_depth_bounds;
  _depth_stencil.minDepthBounds        = max_depth_bounds;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_depth_test_compare_op(vk::CompareOp compare_op) {
  _depth_stencil.depthCompareOp = compare_op;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_stencil_test() {
  _depth_stencil.stencilTestEnable = vk::False;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_descriptor_set_layouts(
    std::span<vk::DescriptorSetLayout> layouts) {
  _pipeline_layout.setLayoutCount = static_cast<uint32_t>(layouts.size());
  _pipeline_layout.pSetLayouts    = layouts.data();
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_descriptor_set_layout(const vk::DescriptorSetLayout& layout) {
  _pipeline_layout.setLayoutCount = 1;
  _pipeline_layout.pSetLayouts    = &layout;
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_push_constant_ranges(
    std::span<vk::PushConstantRange> push_constant_ranges) {
  _pipeline_layout.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
  _pipeline_layout.pPushConstantRanges    = push_constant_ranges.data();
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_input_state(
    std::span<vk::VertexInputBindingDescription> binding_descriptions,
    std::span<vk::VertexInputAttributeDescription> attributes_descriptions) {
  _vertex_input_state.vertexBindingDescriptionCount   = static_cast<uint32_t>(binding_descriptions.size());
  _vertex_input_state.pVertexBindingDescriptions      = binding_descriptions.data();
  _vertex_input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes_descriptions.size());
  _vertex_input_state.pVertexAttributeDescriptions    = attributes_descriptions.data();
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_input_state(
    const vk::VertexInputBindingDescription& binding_descriptions,
    std::span<vk::VertexInputAttributeDescription> attributes_descriptions) {
  _vertex_input_state.vertexBindingDescriptionCount   = 1;
  _vertex_input_state.pVertexBindingDescriptions      = &binding_descriptions;
  _vertex_input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes_descriptions.size());
  _vertex_input_state.pVertexAttributeDescriptions    = attributes_descriptions.data();
  return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::with_input_state(
    const vk::VertexInputBindingDescription& binding_descriptions,
    const vk::VertexInputAttributeDescription& attributes_descriptions) {
  _vertex_input_state.vertexBindingDescriptionCount   = 1;
  _vertex_input_state.pVertexBindingDescriptions      = &binding_descriptions;
  _vertex_input_state.vertexAttributeDescriptionCount = 1;
  _vertex_input_state.pVertexAttributeDescriptions    = &attributes_descriptions;
  return *this;
}

ComputePipelineBuilder::ComputePipelineBuilder() {
  // == Pipeline layouts ===============================================================================================
  _pipeline_layout.setLayoutCount         = 0;
  _pipeline_layout.pushConstantRangeCount = 0;
}

ComputePipelineBuilder ComputePipelineBuilder::create() { return ComputePipelineBuilder(); }

ComputePipelineBuilder& ComputePipelineBuilder::with_shader(vk::ShaderModule compute_shader,
                                                            util::zstring_view entry_point) {
  _shader_stage = vk::PipelineShaderStageCreateInfo{
      .stage  = vk::ShaderStageFlagBits::eCompute,
      .module = compute_shader,
      .pName  = entry_point.empty() ? kDefaultComputeShaderEntryPoint.c_str() : entry_point.c_str(),
  };
  return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_descriptor_set_layouts(
    std::span<vk::DescriptorSetLayout> layouts) {
  _pipeline_layout.setLayoutCount = static_cast<uint32_t>(layouts.size());
  _pipeline_layout.pSetLayouts    = layouts.data();
  return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_descriptor_set_layout(const vk::DescriptorSetLayout& layout) {
  _pipeline_layout.setLayoutCount = 1;
  _pipeline_layout.pSetLayouts    = &layout;
  return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_push_constant_ranges(
    std::span<vk::PushConstantRange> push_constant_ranges) {
  _pipeline_layout.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
  _pipeline_layout.pPushConstantRanges    = push_constant_ranges.data();
  return *this;
}

Result<Pipeline, Error> ComputePipelineBuilder::build(const Device& device) {
  assert(_shader_stage.module && "Compute shader must be provided");

  return device->createPipelineLayout(_pipeline_layout)
      .and_then([&](vk::raii::PipelineLayout&& layout) {
        auto pipeline_info = vk::ComputePipelineCreateInfo{
            .stage  = _shader_stage,
            .layout = layout,
        };

        return device->createComputePipeline(nullptr, pipeline_info)
            .transform([l = std::move(layout)](vk::raii::Pipeline&& p) mutable {
              return Pipeline{
                  .pipeline = std::move(p),
                  .layout   = std::move(l),
              };
            });
      })
      .transform_error([](auto err) {
        return Error{
            .msg     = "Compute Pipeline creation failure",
            .code    = ErrorCode::VulkanObjectCreationFailure{},
            .vk_code = err,
        };
      });
}

}  // namespace eray::vkren
