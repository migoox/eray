#pragma once

namespace eray::vkren {

enum class BufferMemoryType {
  /**
   * @brief The memory is allocated in GPU VRAM and it is not accessible from the CPU.
   */
  DeviceOnly,

  /**
   * @brief Best for CPU-write-once -> GPU-readable. GPU The memory is typically allocated in GPU VRAM but is accessible
   * for mapping to the CPU (Resizable Base Address Register, ReBAR). Since the memory is in VRAM, the shader access is
   * as fast as when using DeviceOnly! It makes it perfect for GPU data that is initialized once.
   *
   * @note https://gpuopen.com/learn/using-d3d12-heap-type-gpu-upload/
   *
   * @warning This feature might not be supported on all platforms!
   * @warning Use `memcpy` to upload full buffer, avoid operations like `buffer[i] += 5`.
   * @warning Avoid creating many small buffers with this memory type. Try to keep at least 64KB of meaningful data in a
   * buffer.
   */
  DeviceUpload,

  /**
   * @brief Best for CPU-write-once -> GPU-read-once scenario. The memory is typically allocated in CPU
   * RAM and transfered via PCIE to the GPU, when read by shader or copied to DeviceOnly buffer. It makes it perfect for
   * staging buffers. This type might be also used for uniform buffers that are updated every frame. 
   *
   * @warning Use `memcpy` to upload full buffer, avoid operations like `buffer[i] += 5` (the memory is not host coherent).
   */
  HostUpload,

  /**
   * @brief Best for GPU-write-once -> CPU-readable scenario. The memory is typically allocated in CPU RAM RAM and
   * transfered via PCIE to the GPU, when read by shader or copied to DeviceOnly buffer.
   */
  HostReadback,
};

}