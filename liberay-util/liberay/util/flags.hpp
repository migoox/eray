#pragma once
#include <type_traits>
namespace eray::util {

template <typename BitType>
class Flags {
 public:
  using BitsType = BitType;
  using MaskType = std::underlying_type_t<BitType>;

  constexpr Flags() noexcept : mask_(0) {}
  constexpr Flags(Flags const& flags) noexcept : mask_(flags.mask_) {}
  constexpr Flags(BitType bit) noexcept : mask_(static_cast<MaskType>(bit)) {}  // NOLINT
  constexpr explicit Flags(MaskType flags) noexcept : mask_(flags) {}

  constexpr Flags<BitType> operator&(Flags<BitType> const& rhs) const noexcept {
    return Flags<BitType>(mask_ & rhs.mask_);
  }

  constexpr Flags<BitType> operator|(Flags<BitType> const& rhs) const noexcept {
    return Flags<BitType>(mask_ | rhs.mask_);
  }

  constexpr Flags<BitType> operator^(Flags<BitType> const& rhs) const noexcept {
    return Flags<BitType>(mask_ ^ rhs.mask_);
  }

  constexpr Flags<BitType>& operator=(Flags<BitType> const& rhs) noexcept = default;
  constexpr Flags<BitType>& operator|=(Flags<BitType> const& rhs) noexcept {
    mask_ |= rhs.mask_;
    return *this;
  }

  constexpr Flags<BitType>& operator&=(Flags<BitType> const& rhs) noexcept {
    mask_ &= rhs.mask_;
    return *this;
  }
  constexpr Flags<BitType>& operator^=(Flags<BitType> const& rhs) noexcept {
    mask_ ^= rhs.mask_;
    return *this;
  }

  constexpr bool has_flag(BitType flag) const noexcept { return (mask_ & static_cast<MaskType>(flag)) != 0; }

 private:
  MaskType mask_;
};

}  // namespace eray::util
