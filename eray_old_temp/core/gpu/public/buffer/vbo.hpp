#pragma once

#include <expected>
#include <liberay/util/memory_region.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>
#include <liberay/vkren/error.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace eray::vkren {

/**
 * @brief Represents a persistently mapped buffer resource that can be used as vertex buffer. Uses additional staging
 * buffers to perform writes to the target vertex buffer.
 */
template <typename TVertex>
struct MappedVertexBuffer {
  eray::vkren::BufferResource staging_buffer;
  void* staging_buffer_map = nullptr;
  eray::vkren::BufferResource vertex_buffer;
  size_t size = 0;

  static eray::vkren::Result<MappedVertexBuffer, eray::vkren::Error> create(eray::vkren::Device& device, size_t _size) {
    vk::DeviceSize size_bytes = _size * sizeof(TVertex);
    auto mapping              = eray::vkren::BufferResource::persistently_mapped_staging_buffer(device, size_bytes);
    if (!mapping) {
      return std::unexpected(mapping.error());
    }

    auto vb = eray::vkren::BufferResource::create_vertex_buffer(device, size_bytes);
    if (!vb) {
      return std::unexpected(vb.error());
    }

    return MappedVertexBuffer<TVertex>{
        .staging_buffer     = std::move(mapping->buffer),
        .staging_buffer_map = mapping->mapped_data,
        .vertex_buffer      = std::move(*vb),
        .size               = _size,
    };
  }

  /**
   * @warning This method must be called when GPU does not read from the UBO, otherwise results in data races.
   * Either use `on_frame_prepare_sync` or create instances for each frame in flight.
   */
  void sync(std::span<TVertex> data) {
    assert(data.size() == size && "Data size must match the vertex buffer size");

    memcpy(staging_buffer_map, data.data(), size * sizeof(TVertex));

    staging_buffer._p_device->immediate_command_submit([this](vk::CommandBuffer cmd) {
      cmd.copyBuffer(staging_buffer.vk_buffer(), vertex_buffer.vk_buffer(),
                     vk::BufferCopy(0, 0, size * sizeof(TVertex)));
    });
  }

  /**
   * @warning This method must be called when GPU does not read from the UBO, otherwise results in data races.
   * Either use `on_frame_prepare_sync` or create instances for each frame in flight.
   */
  void sync(eray::util::MemoryRegion mem_region) {
    assert(mem_region.size_bytes() == size * sizeof(TVertex) && "Data size must match the vertex buffer size");

    memcpy(staging_buffer_map, mem_region.data(), mem_region.size_bytes());

    staging_buffer._p_device->immediate_command_submit([this](vk::CommandBuffer cmd) {
      cmd.copyBuffer(staging_buffer.vk_buffer(), vertex_buffer.vk_buffer(),
                     vk::BufferCopy(0, 0, size * sizeof(TVertex)));
    });
  }

  auto vk_buffer() const { return vertex_buffer.vk_buffer(); }
};

}  // namespace eray::vkren
