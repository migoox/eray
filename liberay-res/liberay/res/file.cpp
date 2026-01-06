#include <expected>
#include <fstream>
#include <liberay/res/file.hpp>
#include <span>

#include "liberay/res/error.hpp"

namespace eray::res {

util::Result<void, FileError> validate_file(const std::filesystem::path& path, std::span<const char*> extensions) {
  if (!std::filesystem::exists(path)) {
    util::Logger::err(R"(Provided file with path "{}" does not exist.)", path.string());
    return std::unexpected(FileError{
        .path = path,
        .msg  = "File does not exist.",
        .code = FileErrorCode::FileNotFound,
    });
  }

  if (std::filesystem::is_directory(path)) {
    util::Logger::err(R"(Provided path "{}" is a directory, not a file.)", path.string());
    return std::unexpected(FileError{
        .path = path,
        .msg  = "Path is a directory, not a file.",
        .code = FileErrorCode::NotAFile,
    });
  }

  if (extensions.empty()) {
    return {};
  }

  auto ext = path.extension().string();
  if (auto it = std::ranges::find(extensions, ext); it == extensions.end()) {
    util::Logger::err(R"(File with path "{}" has invalid extension "{}".)", path.string(), ext);
    return std::unexpected(FileError{
        .path = path,
        .msg  = "Extension is not supported",
        .code = FileErrorCode::InvalidFileExtension,
    });
  }

  return {};
}

util::Result<std::string, FileError> load_as_string_utf8(const std::filesystem::path& path,
                                                         std::span<const char*> extensions) {
  if (auto res = validate_file(path, extensions); !res) {
    return std::unexpected(res.error());
  }

  auto file = std::ifstream(path, std::ios::binary);
  if (!file) {
    return std::unexpected(FileError{
        .path = path,
        .msg  = "Could not open the file stream",
        .code = FileErrorCode::ReadFailure,
    });
  }

  file.seekg(0, std::ios::end);
  auto size = file.tellg();
  file.seekg(0);

  auto str_buff = std::string(static_cast<size_t>(size), '\0');
  file.read(str_buff.data(), size);

  return str_buff;
}

}  // namespace eray::res
