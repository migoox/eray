#include <liberay/vkren/error.hpp>
#include <liberay/vkren/vk_util.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren::vk_util {

void transition_image_barrier(vk::CommandBuffer cmd, vk::Image image, vk::ImageSubresourceRange subresource_range,
                              vk::ImageLayout currentLayout, vk::ImageLayout newLayout) {
  auto image_barrier = vk::ImageMemoryBarrier2{
      .sType = vk::StructureType::eImageMemoryBarrier2,
      .pNext = nullptr,

      .srcStageMask  = vk::PipelineStageFlagBits2::eAllCommands,
      .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
      .dstStageMask  = vk::PipelineStageFlagBits2::eAllCommands,
      .dstAccessMask = vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,

      .oldLayout = currentLayout,
      .newLayout = newLayout,

      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,

      .image            = image,
      .subresourceRange = subresource_range,
  };

  auto dep_info = vk::DependencyInfo{
      .sType                   = vk::StructureType::eDependencyInfo,
      .pNext                   = nullptr,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers    = &image_barrier,
  };

  vkCmdPipelineBarrier2(cmd, dep_info);
}

}  // namespace eray::vkren::vk_util
