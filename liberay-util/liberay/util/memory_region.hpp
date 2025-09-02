#pragma once

#include <cstddef>

namespace eray::util {

/**
 * @brief Represents read-only chunk of memory.
 *
 */
class MemoryRegion {
 public:
  using size_type     = std::size_t;
  using const_pointer = const void*;

  MemoryRegion() = delete;
  MemoryRegion(const_pointer data, size_type size_bytes) : data_(data), size_bytes_(size_bytes) {}

  size_type size_bytes() const { return size_bytes_; }
  const_pointer data() const { return data_; }

 private:
  const_pointer data_{nullptr};
  size_type size_bytes_{0U};
};

}  // namespace eray::util
