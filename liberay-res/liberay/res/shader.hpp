#pragma once

#include <liberay/res/error.hpp>
#include <liberay/util/result.hpp>
#include <span>

namespace eray::res {

/**
 * @brief Represents properly aligned (uint32_t alignment) SPIR-V shader binary code. The binary code size must be a
 * multiple of 4.
 *
 */
struct SPIRVShaderBinary {
  SPIRVShaderBinary() = delete;

  static util::Result<SPIRVShaderBinary, FileError> load_from_path(const std::filesystem::path& path);

  size_t size_bytes() const { return m_.data_bytes.size(); }
  std::span<const char> data_bytes() const { return m_.data_bytes; }
  std::span<const uint32_t> data() const {
    // std::vector default allocator asserts that std::byte array is aligned (the worst case alignment is satisfied)
    return std::span{reinterpret_cast<const uint32_t*>(m_.data_bytes.data()), m_.data_bytes.size() / sizeof(uint32_t)};
  }

 private:
  struct Members {
    std::vector<char> data_bytes;
  } m_;
  explicit SPIRVShaderBinary(Members&& m) : m_(std::move(m)) {}
};

}  // namespace eray::res
