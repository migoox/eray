#pragma once

#include <vulkan/vulkan.hpp>

namespace eray::vkren::vk_util {

/**
 * @brief Invokes `vkCmdPipelineBarrier2`. `cmd` must be in the begin state.
 *
 * @param cmd
 * @param image
 * @param subresource_range
 * @param currentLayout
 * @param newLayout
 */
void transition_image_barrier(vk::CommandBuffer cmd, vk::Image image, vk::ImageSubresourceRange subresource_range,
                              vk::ImageLayout currentLayout, vk::ImageLayout newLayout);

}  // namespace eray::vkren::vk_util
