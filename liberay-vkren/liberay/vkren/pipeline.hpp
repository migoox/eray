#pragma once

#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/shader.hpp>
#include <liberay/vkren/swap_chain.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

struct Pipeline {
  vk::raii::Pipeline pipeline;
  vk::raii::PipelineLayout layout;
};

struct GraphicsPipelineBuilder {
  GraphicsPipelineBuilder() = delete;
  static GraphicsPipelineBuilder create(const SwapChain& swap_chain);

  GraphicsPipelineBuilder& with_shaders(vk::ShaderModule vertex_shader, vk::ShaderModule fragment_shader,
                                        util::zstring_view vertex_shader_entry_point   = "",
                                        util::zstring_view fragment_shader_entry_point = "");

  GraphicsPipelineBuilder& with_primitive_topology(vk::PrimitiveTopology topology,
                                                   bool primitive_restart_enable = false);

  GraphicsPipelineBuilder& with_input_state(std::span<vk::VertexInputBindingDescription> binding_descriptions,
                                            std::span<vk::VertexInputAttributeDescription> attributes_descriptions);
  GraphicsPipelineBuilder& with_input_state(vk::VertexInputBindingDescription& binding_descriptions,
                                            std::span<vk::VertexInputAttributeDescription> attributes_descriptions);
  GraphicsPipelineBuilder& with_input_state(vk::VertexInputBindingDescription& binding_descriptions,
                                            vk::VertexInputAttributeDescription& attributes_descriptions);

  GraphicsPipelineBuilder& with_polygon_mode(vk::PolygonMode polygon_mode, float line_width = 1.F);
  GraphicsPipelineBuilder& with_cull_mode(vk::CullModeFlags cull_mode, vk::FrontFace front_face);
  GraphicsPipelineBuilder& with_depth_bias(float slope_factor);

  /**
   * @brief Sets rasterization samples.
   *
   * @warning If the swapchain was provided during builder creation, the rasterization samples are already set based on
   * the swapchain.
   *
   * @param rasterization_samples
   * @return GraphicsPipelineBuilder&
   */
  GraphicsPipelineBuilder& with_multisampling(vk::SampleCountFlagBits rasterization_samples);

  /**
   * @brief If sampling shading is enabled, an implementation must invoke the fragment shader at least
   * min_sample_shading*rasterization_samples times per fragment.
   *
   * @param min_sample_shading
   * @return GraphicsPipelineBuilder&
   */
  GraphicsPipelineBuilder& with_sample_shading(float min_sample_shading);

  GraphicsPipelineBuilder& with_depth_test(bool test_write = true);
  GraphicsPipelineBuilder& with_depth_test_compare_op(vk::CompareOp compare_op);
  GraphicsPipelineBuilder& with_depth_bounds_test(float min_depth_bounds, float max_depth_bounds);

  GraphicsPipelineBuilder& with_stencil_test();

  GraphicsPipelineBuilder& with_src_blend_factors(vk::BlendFactor color_blend_factor,
                                                  vk::BlendFactor alpha_blend_factor);
  GraphicsPipelineBuilder& with_dst_blend_factors(vk::BlendFactor color_blend_factor,
                                                  vk::BlendFactor alpha_blend_factor);
  GraphicsPipelineBuilder& with_blend_ops(vk::BlendOp color_blend_op, vk::BlendOp alpha_blend_op);
  GraphicsPipelineBuilder& with_color_write_mask(vk::ColorComponentFlags flags);
  GraphicsPipelineBuilder& with_blending();

  GraphicsPipelineBuilder& with_descriptor_set_layouts(std::span<vk::DescriptorSetLayout> layout);
  GraphicsPipelineBuilder& with_descriptor_set_layout(vk::DescriptorSetLayout& layouts);
  GraphicsPipelineBuilder& with_push_constant_ranges(std::span<vk::PushConstantRange> push_constant_ranges);

  GraphicsPipelineBuilder& with_constant_ranges();

  Result<Pipeline, Error> build(const Device& device);

  std::vector<vk::PipelineShaderStageCreateInfo> _shader_stages;
  std::vector<vk::DynamicState> _dynamic_states;
  vk::PipelineViewportStateCreateInfo _viewport_state;
  vk::PipelineInputAssemblyStateCreateInfo _input_assembly;
  vk::PipelineVertexInputStateCreateInfo _vertex_input_state;
  vk::PipelineRasterizationStateCreateInfo _rasterizer;
  vk::PipelineMultisampleStateCreateInfo _multisampling;
  vk::PipelineDepthStencilStateCreateInfo _depth_stencil;
  vk::PipelineColorBlendAttachmentState _color_blend;
  vk::PipelineLayoutCreateInfo _pipeline_layout;
  vk::Format _color_attachment_format;
  vk::Format _depth_stencil_format;

  static constexpr util::zstring_view kDefaultVertexShaderEntryPoint   = "mainVert";
  static constexpr util::zstring_view kDefaultFragmentShaderEntryPoint = "mainFrag";

 private:
  explicit GraphicsPipelineBuilder(const SwapChain& swap_chain);
};

// TODO(migoox): Implement ComputePipelineBuilder
class ComputePipelineBuilder {
  void set_shaders();
};

}  // namespace eray::vkren
