#pragma once

#include <liberay/vkren/common.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/image_description.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

/**
 * @brief Allows for single time rendering to an image.
 *
 */
struct OffscreenFragmentRenderer {
  struct TargetInfo {
    ImageResource img;
    vk::raii::ImageView img_view      = nullptr;
    vk::raii::Framebuffer framebuffer = nullptr;
  };
  static constexpr size_t kTargetCount = 2;
  std::array<TargetInfo, kTargetCount> targets_;
  size_t current_image_;

  vk::raii::RenderPass render_pass_         = nullptr;
  vk::raii::CommandPool cmd_pool_           = nullptr;
  vk::raii::CommandBuffer cmd_buff_         = nullptr;
  vk::raii::Pipeline pipeline_              = nullptr;
  vk::raii::PipelineLayout pipeline_layout_ = nullptr;
  vk::raii::Semaphore finished_semaphore_   = nullptr;
  vk::raii::Fence finished_fence_           = nullptr;
  observer_ptr<const Device> _p_device      = nullptr;
  bool blocking{true};
  vk::Viewport viewport;

  static Result<OffscreenFragmentRenderer, Error> create(Device& device, const ImageDescription& target_image_desc,
                                                         bool blocking = true);

  // TODO(migoox): generate the vertex_module bytecode
  void init_pipeline(vk::ShaderModule vertex_module, vk::ShaderModule fragment_module,
                     vk::DescriptorSetLayout descriptor_set_layout);

  vk::Image target_image() const { return targets_[current_image_].img.vk_image(); }
  vk::ImageView target_image() { return targets_[current_image_].img_view; }
  vk::ImageView back_image() { return targets_[(current_image_ + 1) % kTargetCount].img_view; }
  vk::ImageView image(size_t i) { return targets_[i % kTargetCount].img_view; }

  size_t current_image_ind() const { return current_image_; }
  size_t next_image_ind() const { return (current_image_ + 1) % kTargetCount; }

  void set_viewport(int x, int y, int width, int height);

  /**
   * @brief Expects the target image to be in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL. After execution, this image is in
   * the  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
   *
   * @param descriptor_set
   */
  void render_once(vk::DescriptorSet descriptor_set, vk::ClearColorValue = {1.F, 1.F, 1.F, 1.F});
  void clear(vk::ClearColorValue = {1.F, 1.F, 1.F, 1.F});

  vk::Semaphore finished_semaphore() const { return finished_semaphore_; }
};

}  // namespace eray::vkren
