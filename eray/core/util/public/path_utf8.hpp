#pragma once
#include <filesystem>
#include <liberay/util/zstring_view.hpp>
#include <string>

namespace eray::util {

/**
 * @brief Convert the system agnostic path to UTF8 string. Since Windows paths are stored as wide chars, this function
 * is extremely useful when dealing with external libraries that expect UTF8 strings.
 *
 * @param path
 * @return std::string
 */
std::string path_to_utf8str(const std::filesystem::path& path);

/**
 * @brief Convert the UTF8 string to a system agnostic path. Extremely usefull when dealing with external libraries.
 *
 * @param str_path
 * @return std::filesystem::path
 */
std::filesystem::path utf8str_to_path(zstring_view str_path);

}  // namespace eray::util
