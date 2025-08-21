#pragma once

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <liberay/res/error.hpp>
#include <liberay/util/result.hpp>
#include <liberay/util/ruleof.hpp>
#include <vector>

namespace eray::res {

using ColorU32         = uint32_t;
using ColorComponentU8 = uint8_t;

class Color {
 public:
  static ColorU32 from_rgb_norm(float r, float g, float b) {
    return (static_cast<uint32_t>(std::round(r * 255.F)) & 0xFF) |
           ((static_cast<uint32_t>(std::round(g * 255.F)) & 0xFF) << 8) |
           ((static_cast<uint32_t>(std::round(b * 255.F)) & 0xFF) << 16) | 0xFF000000;
  }

  static ColorU32 from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<uint32_t>(r)) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) | 0xFF000000;
  }

  static ColorU32 from_rgba_norm(float r, float g, float b, float a) {
    return (static_cast<uint32_t>(std::round(r * 255.F)) & 0xFF) |
           ((static_cast<uint32_t>(std::round(g * 255.F)) & 0xFF) << 8) |
           ((static_cast<uint32_t>(std::round(b * 255.F)) & 0xFF) << 16) |
           ((static_cast<uint32_t>(std::round(a * 255.F)) & 0xFF) << 24);
  }

  static ColorU32 from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<uint32_t>(r)) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(a) << 24);
  }

  static float red_norm(uint32_t color) { return static_cast<float>(color & 0xFF) / 255.F; }
  static ColorComponentU8 red(uint32_t color) { return static_cast<uint8_t>(color & 0xFF); }

  static float green_norm(uint32_t color) { return static_cast<float>((color >> 8) & 0xFF) / 255.F; }
  static ColorComponentU8 green(uint32_t color) { return static_cast<uint8_t>((color >> 8) & 0xFF); }

  static float blue_norm(uint32_t color) { return static_cast<float>((color >> 16) & 0xFF) / 255.F; }
  static ColorComponentU8 blue(uint32_t color) { return static_cast<uint8_t>((color >> 16) & 0xFF); }

  static float alpha_norm(uint32_t color) { return static_cast<float>((color >> 24) & 0xFF) / 255.F; }
  static ColorComponentU8 alpha(uint32_t color) { return static_cast<uint8_t>((color >> 24) & 0xFF); }

  static constexpr ColorU32 kBlack = 0xFF000000;
};

struct MipMappedImage;

/**
 * @brief Represents an image with alpha channel (4 bytes per pixel). The pixel format is R8 G8 B8 A8. The data is
 * stored in RAM.
 *
 */
class Image {
 public:
  ERAY_DEFAULT_MOVE_AND_COPY_CTOR(Image)
  ERAY_DEFAULT_MOVE_AND_COPY_ASSIGN(Image)

  static Image create(uint32_t width, uint32_t height, ColorU32 color = Color::kBlack);
  static Image create(uint32_t width, uint32_t height, uint8_t bpp, std::vector<ColorU32>&& data);

  static util::Result<Image, FileError> load_from_path(const std::filesystem::path& path);

  bool is_in_bounds(uint32_t x, uint32_t y) const;
  void set_pixel(uint32_t x, uint32_t y, ColorU32 color);
  void set_pixel_safe(uint32_t x, uint32_t y, ColorU32 color);
  ColorU32 pixel(uint32_t x, uint32_t y) const;
  void resize(uint32_t new_width, uint32_t new_height, ColorU32 color = Color::kBlack);
  void clear(ColorU32 color);

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }
  size_t size_in_bytes() const { return width_ * height_ * sizeof(uint32_t); }

  const ColorU32* raw() const { return data_.data(); }
  const ColorComponentU8* raw_bytes() const { return reinterpret_cast<const ColorComponentU8*>(data_.data()); }

  /**
   * @brief Calculates the number of mip levels basing on height and width of the image.
   *
   * @return uint32_t mip levels count
   */
  uint32_t calculate_mip_levels() const;
  static uint32_t calculate_mip_levels(uint32_t width, uint32_t height);

  /**
   * @brief CPU-sided mipmaps generation. Returns a buffer of packed images with LOD ranging 0 to mip levels - 1.
   *
   */
  MipMappedImage generate_mipmaps_buffer();

 private:
  Image();
  Image(uint32_t width, uint32_t height, ColorU32 color = Color::kBlack);
  Image(uint32_t width, uint32_t height, uint8_t bpp, std::vector<ColorU32>&& data);

  uint32_t width_;
  uint32_t height_;
  uint8_t bpp_ = 4;

  std::vector<ColorU32> data_;
};

struct MipMappedImage {
  std::vector<ColorU32> data;
  uint32_t lod0_width;
  uint32_t lod0_height;
  uint32_t mip_levels;
  uint8_t bpp = 4;

  size_t size_in_bytes() const { return data.size() * sizeof(uint32_t); }
  const ColorU32* raw() const { return data.data(); }
  const ColorComponentU8* raw_bytes() const { return reinterpret_cast<const ColorComponentU8*>(data.data()); }
};
}  // namespace eray::res
