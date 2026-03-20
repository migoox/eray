#pragma once
#include <filesystem>
#include <liberay/res/error.hpp>
#include <liberay/util/result.hpp>
#include <span>

namespace eray::res {

/**
 * @brief File is valid if it exists and is not a directory. If extensions are provided, the function checks if the
 * file has expected extension.
 *
 * @param path
 * @param extensions Extension must include the dot (`.`), e.g. `.gltf`.
 * @return util::Result<void, FileError>
 */
util::Result<void, FileError> validate_file(const std::filesystem::path& path, std::span<const char*> extensions = {});

util::Result<std::string, FileError> load_as_string_utf8(const std::filesystem::path& path,
                                                         std::span<const char*> extensions = {});

}  // namespace eray::res
