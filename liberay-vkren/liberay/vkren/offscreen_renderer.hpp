#pragma once

#include <liberay/vkren/common.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace eray::vkren {

/**
 * @brief Allows for single time rendering to an image.
 *
 */
struct OffscreenFragmentRenderer {
  ImageResource target_img_;
  vk::raii::ImageView target_img_view_      = nullptr;
  vk::raii::RenderPass render_pass_         = nullptr;
  vk::raii::Framebuffer framebuffer_        = nullptr;
  vk::raii::Fence fence_                    = nullptr;
  vk::raii::CommandPool cmd_pool_           = nullptr;
  vk::raii::CommandBuffer cmd_buff_         = nullptr;
  vk::raii::Pipeline pipeline_              = nullptr;
  vk::raii::PipelineLayout pipeline_layout_ = nullptr;
  observer_ptr<const Device> _p_device      = nullptr;

  static Result<OffscreenFragmentRenderer, Error> create(Device& device, const ImageDescription& target_image_desc);

  // TODO(migoox): generate the vertex_module bytecode
  void init_pipeline(vk::ShaderModule vertex_module, vk::ShaderModule fragment_module,
                     vk::DescriptorSetLayout descriptor_set_layout);

  vk::Image target_image() const { return target_img_._image._vk_handle; }
  vk::ImageView target_image_view() const { return target_img_view_; }

  /**
   * @brief Expects the target image to be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL. After execution, this image is in
   * the  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
   *
   * @param descriptor_set
   */
  void render_once(vk::DescriptorSet descriptor_set, vk::ClearColorValue = {1.F, 1.F, 1.F, 1.F});
  void clear(vk::ClearColorValue = {1.F, 1.F, 1.F, 1.F});
};

}  // namespace eray::vkren
