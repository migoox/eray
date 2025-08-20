#include <liberay/res/file.hpp>

namespace eray::res {

util::Result<void, FileError> validate_file(const std::filesystem::path& path, std::span<const char*> extensions) {
  if (!std::filesystem::exists(path)) {
    util::Logger::err(R"(Provided image file with path "{}" does not exist.)", path.string());
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
}  // namespace eray::res
