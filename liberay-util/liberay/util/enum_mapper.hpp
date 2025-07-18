#pragma once

#include <array>
#include <cstddef>
#include <liberay/util/zstring_view.hpp>
#include <optional>
#include <type_traits>

namespace eray::util {

template <typename T>
concept CEnumWithCount = requires {
  std::is_enum_v<std::remove_cv_t<std::remove_reference_t<T>>>;
  { T::_Count } -> std::convertible_to<T>;
};

template <typename T>
concept CEnumMappingValue = requires { std::is_move_assignable<T>(); };

template <CEnumWithCount EnumType, CEnumMappingValue Value>
class EnumMapper;

template <CEnumWithCount EnumType, typename Value>
class EnumMapperIterator {
 public:
  using enum_type = typename EnumMapper<EnumType, Value>::enum_type;

  EnumMapperIterator(const EnumMapper<EnumType, Value>& mapping, size_t index) : mapping_(mapping), index_(index) {}

  std::pair<enum_type, Value> operator*() const {
    return {static_cast<enum_type>(index_), mapping_[static_cast<enum_type>(index_)]};
  }

  EnumMapperIterator& operator++() {
    ++index_;
    return *this;
  }

  EnumMapperIterator operator++(int) {
    EnumMapperIterator temp = *this;
    ++(*this);
    return temp;
  }

  bool operator==(const EnumMapperIterator& other) const { return index_ == other.index_; }
  bool operator!=(const EnumMapperIterator& other) const { return !(*this == other); }

 private:
  const EnumMapper<EnumType, Value>& mapping_;  // NOLINT
  size_t index_;
};

template <CEnumWithCount EnumType, CEnumMappingValue Value>
class EnumMapper {
 public:
  using enum_type                   = std::remove_cv_t<std::remove_reference_t<EnumType>>;
  static constexpr size_t kEnumSize = static_cast<size_t>(EnumType::_Count);

  EnumMapper() = delete;

  template <size_t N>
  consteval explicit EnumMapper(const std::pair<EnumType, Value> (&values_map)[N]) {
    static_assert(N == kEnumSize, "Names count should be the same as enum entries count.");

    for (size_t i = 0; i < kEnumSize; ++i) {
      present_[i] = false;
    }

    for (size_t i = 0; i < kEnumSize; ++i) {
      auto idx      = static_cast<size_t>(values_map[i].first);
      names_[idx]   = values_map[i].second;
      present_[idx] = true;
    }

    validate_mapping(std::make_index_sequence<kEnumSize>{});
  }

  constexpr auto begin() const { return EnumMapperIterator<EnumType, Value>(*this, 0); }
  constexpr auto end() const { return EnumMapperIterator<EnumType, Value>(*this, kEnumSize); }

  constexpr Value value(enum_type enum_entry) const { return names_[static_cast<size_t>(enum_entry)]; }
  constexpr Value operator[](enum_type enum_entry) const { return names_[static_cast<size_t>(enum_entry)]; }

  // Keep in mind this mapping may be neither injective nor surjective
  constexpr std::optional<enum_type> from_value(Value value) const {
    for (size_t i = 0; i < kEnumSize; ++i) {
      if (names_[i] == value) {
        return std::make_optional(static_cast<enum_type>(i));
      }
    }

    return std::nullopt;
  }

 private:
  template <size_t... Is>
  consteval void validate_mapping(std::index_sequence<Is...>) const {
    ((check_present(Is)), ...);
  }

  consteval void check_present(size_t idx) const {
    if (!present_[idx]) {
      throw "Missing mapping for enum value";
    }
  }

  std::array<Value, kEnumSize> names_  = {};
  std::array<bool, kEnumSize> present_ = {};
};

template <CEnumWithCount EnumType>
using StringEnumMapper = EnumMapper<EnumType, zstring_view>;

}  // namespace eray::util
