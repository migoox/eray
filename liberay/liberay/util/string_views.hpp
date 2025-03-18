#pragma once
#include <ranges>
#include <string_view>

namespace eray::util {

inline auto make_lines_view(std::string_view str) {
  return str | std::views::split('\n') | std::views::transform([](auto&& r) {
           // Handle CR LF to store \n newlines only
           auto line_str = std::string_view(r);
           if (line_str.ends_with('\r')) {
             line_str = line_str.substr(0, line_str.size() - 1);
           }
           return line_str;
         });
}

inline auto make_words_view(std::string_view str) {
  return str | std::views::split(' ') | std::views::transform([](auto&& r) { return std::string_view(r); }) |
         std::views::filter([](auto chunk) { return !chunk.empty(); });
}

using WordsStringViewIterator = std::ranges::iterator_t<decltype(make_words_view(std::string_view{}))>;

}  // namespace eray::util
