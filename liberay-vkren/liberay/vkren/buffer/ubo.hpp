#pragma once
#include <expected>
#include <liberay/math/mat.hpp>
#include <liberay/math/vec.hpp>
#include <liberay/util/memory_region.hpp>
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/device.hpp>

namespace eray::vkren {

/**
 * @brief Represents a persistently mapped buffer resource used as uniform buffer.
 */
template <typename TUniformBuffer>
struct MappedUniformBuffer {
  eray::vkren::BufferResource ubo_gpu;
  void* ubo_map = nullptr;
  bool _dirty   = true;

  static eray::vkren::Result<MappedUniformBuffer<TUniformBuffer>, eray::vkren::Error> create(
      eray::vkren::Device& device) {
    vk::DeviceSize size_bytes = sizeof(TUniformBuffer);
    auto result = eray::vkren::BufferResource::create_persistently_mapped_uniform_buffer(device, size_bytes);
    if (!result) {
      return std::unexpected(result.error());
    }

    MappedUniformBuffer<TUniformBuffer> mubo;
    mubo.ubo_gpu = std::move(result->buffer);
    mubo.ubo_map = result->mapped_data;
    return mubo;
  }

  /**
   * @warning This method must be called when GPU does not read from the UBO, otherwise results in data races.
   * Either use `on_frame_prepare_sync` or create instances for each frame in flight.
   */
  void sync(TUniformBuffer& data) {
    if (_dirty) {
      memcpy(ubo_map, &data, sizeof(TUniformBuffer));
    }
    _dirty = false;
  }

  /**
   * @warning This method must be called when GPU does not read from the UBO, otherwise results in data races.
   * Either use `on_frame_prepare_sync` or create instances for each frame in flight.
   */
  void sync(void* data, size_t size_bytes) {
    if (_dirty) {
      memcpy(ubo_map, data, size_bytes);
    }
    _dirty = false;
  }

  /**
   * @warning This method must be called when GPU does not read from the UBO, otherwise results in data races.
   * Either use `on_frame_prepare_sync` or create instances for each frame in flight.
   */
  void sync(eray::util::MemoryRegion mem_region) {
    if (_dirty) {
      memcpy(ubo_map, mem_region.data(), mem_region.size_bytes());
    }
    _dirty = false;
  }

  auto desc_buffer_info() const { return ubo_gpu.desc_buffer_info(); }

  void mark_dirty() { _dirty = true; }
};

}  // namespace eray::vkren
