#include <liberay/util/path_utf8.hpp>

namespace eray::util {

std::string path_to_utf8str(const std::filesystem::path& path) {
#ifdef IS_WINDOWS
  std::wstring wide_path = path.wstring();
  int size               = WideCharToMultiByte(CP_UTF8, 0, wide_path.c_str(), -1, nullptr, 0, nullptr, nullptr);
  std::string utf8_path(static_cast<size_t>(size), 0);
  WideCharToMultiByte(CP_UTF8, 0, wide_path.c_str(), -1, utf8_path.data(), size, nullptr, nullptr);
  return utf8_path;
#else
  return path.string();
#endif
}

std::filesystem::path utf8str_to_path(zstring_view str_path) {
#ifdef IS_WINDOWS
  const char* raw_path = str_path.data();
  int raw_length       = static_cast<int>(str_path.length());
  int size             = MultiByteToWideChar(CP_UTF8, 0, raw_path, raw_length, nullptr, 0);
  std::wstring wide_path(static_cast<size_t>(size), 0);
  MultiByteToWideChar(CP_UTF8, 0, raw_path, raw_length, wide_path.data(), size);
  return std::filesystem::path(wide_path);
#else
  return std::filesystem::path(std::string_view(str_path));
#endif
}

}  // namespace eray::util
