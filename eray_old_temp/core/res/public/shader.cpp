#include <expected>
#include <liberay/res/error.hpp>
#include <liberay/res/file.hpp>
#include <liberay/res/shader.hpp>

namespace eray::res {

util::Result<SPIRVShaderBinary, FileError> SPIRVShaderBinary::load_from_path(const std::filesystem::path& path) {
  if (auto result = validate_file(path); !result) {
    return std::unexpected(result.error());
  }

  auto file = std::ifstream(path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    eray::util::Logger::err("Unable to open a stream for file {}", path.string());
    return std::unexpected(FileError{
        .path = path,
        .msg  = "Stream failure",
        .code = FileErrorCode::PermissionDenied,
    });
  }

  auto bytes = static_cast<size_t>(file.tellg());
  if (bytes % 4 != 0) {
    eray::util::Logger::err("SPIR-V file size {} is not a multiple of 4", bytes);
    return std::unexpected(FileError{
        .path = path,
        .msg  = "Invalid SPIR-V file size",
        .code = FileErrorCode::IncorrectFormat,
    });
  }

  auto buffer = std::vector<char>(bytes);
  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();
  if (file.bad()) {
    eray::util::Logger::warn("File {} was not closed properly", path.string());
  }

  eray::util::Logger::info("Read {} bytes from {}", bytes, path.string());

  return SPIRVShaderBinary(Members{
      .data_bytes = buffer,
  });
}

}  // namespace eray::res
