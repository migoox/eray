#pragma once
#include <filesystem>

namespace eray::res {

enum class FileErrorCode : uint8_t {
  FileNotFound         = 0,
  NotAFile             = 1,
  ReadFailure          = 2,
  FileTooLarge         = 3,
  PermissionDenied     = 4,
  IncorrectFormat      = 5,
  InvalidFileExtension = 6,
};

struct FileError {
  std::filesystem::path path;
  std::string msg;
  FileErrorCode code;
};

}  // namespace eray::res
