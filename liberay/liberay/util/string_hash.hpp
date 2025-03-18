#pragma once

#include <string>
#include <string_view>

namespace eray::util {

struct StringHash {
  using is_transparent = void;

  [[nodiscard]] size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }
  [[nodiscard]] size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
  [[nodiscard]] size_t operator()(const std::string& txt) const { return std::hash<std::string>{}(txt); }
};

}  // namespace eray::util
