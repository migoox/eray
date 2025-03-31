// MIT License
//
// Copyright(c)
// Microsoft Corporation. All rights reserved.
//
//     Permission is hereby granted,
//     free of charge, to any person obtaining a copy of this software and associated documentation files(the
//     "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
//     modify, merge, publish, distribute, sublicense, and / or sell copies of the Software, and to permit persons to
//     whom the Software is furnished to do so, subject to the following conditions :
//
//     The above copyright notice and this permission notice shall be included in all copies
//     or
//     substantial portions of the Software.
//
//     THE SOFTWARE IS PROVIDED "AS IS",
//     WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//     FITNESS FOR A PARTICULAR PURPOSE AND
//     NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//     DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//     OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE
//

#pragma once

#include <cassert>
#include <format>
#include <liberay/util/panic.hpp>
#include <string_view>

template <class TChar>
// NOLINTBEGIN
class basic_zstring_view : public std::basic_string_view<TChar> {
  using size_type = typename std::basic_string_view<TChar>::size_type;

 public:
  constexpr basic_zstring_view() noexcept                                     = default;
  constexpr basic_zstring_view(const basic_zstring_view&) noexcept            = default;
  constexpr basic_zstring_view& operator=(const basic_zstring_view&) noexcept = default;

  constexpr basic_zstring_view(const TChar* pStringData, size_type stringLength) noexcept
      : std::basic_string_view<TChar>(pStringData, stringLength) {
    if (pStringData[stringLength] != 0) {
      eray::util::panic("Could not create zstring_view -- the string must be null-terminated.");
    }
  }

  template <size_t stringArrayLength>
  constexpr basic_zstring_view(const TChar (&stringArray)[stringArrayLength]) noexcept
      : std::basic_string_view<TChar>(&stringArray[0], length_n(&stringArray[0], stringArrayLength)) {}

  // Construct from null-terminated char ptr. To prevent this from overshadowing array construction,
  // we disable this constructor if the value is an array (including string literal).
  template <typename TPtr>
  constexpr basic_zstring_view(TPtr&& pStr) noexcept
    requires(std::is_convertible_v<TPtr, const TChar*> && !std::is_array_v<TPtr>)
      : std::basic_string_view<TChar>(std::forward<TPtr>(pStr)) {}

  constexpr basic_zstring_view(const std::basic_string<TChar>& str) noexcept
      : std::basic_string_view<TChar>(&str[0], str.size()) {}

  // basic_string_view [] precondition won't let us read view[view.size()]; so we define our own.
  [[nodiscard]] constexpr const TChar& operator[](size_type idx) const noexcept {
    assert(idx <= this->size() && this->data() != nullptr);
    return this->data()[idx];
  }

  [[nodiscard]] constexpr const TChar* c_str() const noexcept {
    assert(this->data() == nullptr || this->data()[this->size()] == 0);
    return this->data();
  }

  // Inside the public section of basic_zstring_view
  constexpr operator std::basic_string_view<TChar>() const noexcept {
    return std::basic_string_view<TChar>(this->data(), this->size());
  }

 private:
  // Bounds-checked version of char_traits::length, like strnlen. Requires that the input contains a null terminator.
  static constexpr size_type length_n(const TChar* str, size_type buf_size) noexcept {
    const std::basic_string_view<TChar> view(str, buf_size);
    auto pos = view.find_first_of(TChar());
    if (pos == view.npos) {
      eray::util::panic("Length of zstring_view failed -- the string must be null-terminated.");
    }
    return pos;
  }

  // The following basic_string_view methods must not be allowed because they break the null-termination.
  using std::basic_string_view<TChar>::swap;
  using std::basic_string_view<TChar>::remove_suffix;
};

template <class TChar>
struct std::formatter<basic_zstring_view<TChar>, TChar> : std::formatter<std::basic_string_view<TChar>, TChar> {
  template <typename FormatContext>
  auto format(const basic_zstring_view<TChar>& zv, FormatContext& ctx) const {
    return std::formatter<std::basic_string_view<TChar>, TChar>::format(
        std::basic_string_view<TChar>(zv.data(), zv.size()), ctx);
  }
};

/**
 * A zstring_view is identical to a std::string_view except it is always null-terminated (unless empty).
 * zstring_view can be used for storing string literals without "forgetting" the length or that it is null-terminated.
 * A zstring_view can be converted implicitly to a std::string_view because it is always safe to use a null-terminated
 * string_view as a plain string view.
 * A zstring_view can be constructed from a std::string because the data in std::string is null-terminated.
 *
 * +--------------------------------+---------------+------------------+-------------+--------------------+
 * | Feature                        | zstring_view  | std::string_view | const char* | const std::string& |
 * +--------------------------------+---------------+------------------+-------------+--------------------+
 * | Null-termination guarantee     |     ✅ Yes    |      ❌ No       |    ❌ No    |      ✅ Yes        |
 * | Stores string length           |     ✅ Yes    |      ✅ Yes      |    ❌ No    |      ✅ Yes        |
 * | Avoids copying string data     |     ✅ Yes    |      ✅ Yes      |    ✅ Yes   |      ❌ No         |
 * | Bounds-checked access          |     ✅ Yes    |      ❌ No       |    ❌ No    |      ✅ Yes        |
 * | Compatible with C APIs         |     ✅ Yes    |      ❌ No       |    ✅ Yes   |      ✅ Yes        |
 * +--------------------------------+---------------+------------------+-------------+--------------------+
 */
using zstring_view  = basic_zstring_view<char>;
using zwstring_view = basic_zstring_view<wchar_t>;

namespace std {
template <>
struct hash<basic_zstring_view<char>> {
  size_t operator()(const basic_zstring_view<char>& txt) const noexcept { return hash<std::string_view>{}(txt); }
};
}  // namespace std

// NOLINTEND
