#pragma once
#include <liberay/vkren/buffer.hpp>
#include <liberay/vkren/device.hpp>

namespace sim {

struct LineStripRingBufferFrameData {
  eray::vkren::BufferResource vertex_buffer;
  eray::vkren::BufferResource staging_buffer;
  void* staging_buffer_mapping = nullptr;
  bool dirty                   = true;
};

/**
 * @brief Persistently mapped line strip buffer compatible with frame in flights. The vertices are kept in the
 * DEVICE_LOCAL buffer. If max count is exceeded the line strip wraps around (ring buffer).
 *
 */
template <typename TVertex>
struct LineStripRingBuffer {
  std::vector<LineStripRingBufferFrameData> frame_data;
  std::uint32_t max_size = 0;
  std::vector<TVertex> points;

  std::uint32_t _pivot = 0;
  bool _rounded        = false;

  /**
   * @brief Creates the buffer
   *
   * @param device
   * @param max_size_
   * @param max_frames_in_flight If frames in flight are disabled, use 1.
   * @return LineStripRingBuffer
   */
  static LineStripRingBuffer create(eray::vkren::Device& device, std::uint32_t max_size_,
                                    std::uint32_t max_frames_in_flight) {
    vk::DeviceSize size_bytes = max_size_ * sizeof(TVertex);

    auto new_frame_data = std::vector<LineStripRingBufferFrameData>();
    new_frame_data.resize(max_frames_in_flight);

    for (auto i = 0U; i < max_frames_in_flight; ++i) {
      auto mapping = eray::vkren::BufferResource::persistently_mapped_staging_buffer(device, size_bytes)
                         .or_panic("Could not create a staging buffer for line strip");
      auto vb = eray::vkren::BufferResource::create_vertex_buffer(device, size_bytes)
                    .or_panic("Could not create a vertex buffer for line strip");

      new_frame_data[i].staging_buffer         = std::move(mapping.buffer);
      new_frame_data[i].staging_buffer_mapping = std::move(mapping.mapped_data);
      new_frame_data[i].vertex_buffer          = std::move(vb);
      new_frame_data[i].dirty                  = false;
    }

    auto new_points = std::vector<TVertex>();
    new_points.resize(max_size_);

    return LineStripRingBuffer{
        .frame_data = std::move(new_frame_data),
        .max_size   = max_size_,
        .points     = std::move(new_points),
        ._pivot     = 1,
        ._rounded   = false,
    };
  }

  void clear() {
    _pivot   = 1;
    _rounded = false;
  }

  //   void resize(std::uint32_t max_size_); // TODO(migoox): implement this

  void push_vertex(const TVertex& point) {
    points[_pivot] = point;
    ++_pivot;
    if (_pivot >= max_size) {
      points[0] = point;
      _pivot    = 1;
      _rounded  = true;
    }

    for (auto& fd : frame_data) {
      fd.dirty = true;
    }
  }

  void update(std::uint32_t image_index) {
    if (frame_data[image_index].dirty) {
      memcpy(frame_data[image_index].staging_buffer_mapping, points.data(), size_bytes());
      auto region = eray::util::MemoryRegion{points.data(), size_bytes()};
      frame_data[image_index].vertex_buffer._p_device->immediate_command_submit([&](vk::CommandBuffer cmd) {
        cmd.copyBuffer(frame_data[image_index].staging_buffer.vk_buffer(),
                       frame_data[image_index].vertex_buffer.vk_buffer(), vk::BufferCopy(0, 0, size_bytes()));
      });
      frame_data[image_index].dirty = false;
    }
  }

  vk::DeviceSize size_bytes() const { return max_size * sizeof(TVertex); }

  void render(vk::raii::CommandBuffer& graphics_command_buffer, std::uint32_t image_index) const {
    graphics_command_buffer.bindVertexBuffers(0, frame_data[image_index].vertex_buffer.vk_buffer(), {0});
    if (_rounded) {
      graphics_command_buffer.draw(_pivot, 1, 0, 0);
      graphics_command_buffer.draw(max_size - _pivot, 1, _pivot, 0);
    } else {
      graphics_command_buffer.draw(_pivot - 1, 1, 1, 0);
    }
  }
};

}  // namespace sim
