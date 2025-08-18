#pragma once

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <liberay/util/result.hpp>
#include <liberay/util/ruleof.hpp>
#include <vector>

namespace eray::res {

using ColorU32         = uint32_t;
using ColorComponentU8 = uint8_t;

class Color {
 public:
  static ColorU32 from_rgb_norm(float r, float g, float b) {
    return static_cast<uint32_t>(std::round(r * 255.F)) << 24 | static_cast<uint32_t>(std::round(g * 255.F)) << 16 |
           static_cast<uint32_t>(std::round(b * 255.F)) << 8 | 0xFF;
  }

  static ColorU32 from_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint32_t>(r) << 24 | static_cast<uint32_t>(g) << 16 | static_cast<uint32_t>(b) << 8 | 0xFF;
  }

  static ColorU32 from_rgba_norm(float r, float g, float b, float a) {
    return static_cast<uint32_t>(std::round(r * 255.F)) << 24 | static_cast<uint32_t>(std::round(g * 255.F)) << 16 |
           static_cast<uint32_t>(std::round(b * 255.F)) << 8 | static_cast<uint32_t>(std::round(a * 255.F));
  }

  static ColorU32 from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return static_cast<uint32_t>(r) << 24 | static_cast<uint32_t>(g) << 16 | static_cast<uint32_t>(b) << 8 |
           static_cast<uint32_t>(a);
  }

  static float red_norm(uint32_t color) { return static_cast<float>((color >> 24) & 0xFF) / 255.F; }
  static ColorComponentU8 red(uint32_t color) { return static_cast<uint8_t>((color >> 24) & 0xFF); }

  static float green_norm(uint32_t color) { return static_cast<float>((color >> 16) & 0xFF) / 255.F; }
  static ColorComponentU8 green(uint32_t color) { return static_cast<uint8_t>((color >> 16) & 0xFF); }

  static float blue_norm(uint32_t color) { return static_cast<float>((color >> 8) & 0xFF) / 255.F; }
  static ColorComponentU8 blue(uint32_t color) { return static_cast<uint8_t>((color >> 8) & 0xFF); }

  static float alpha_norm(uint32_t color) { return static_cast<float>(color & 0xFF) / 255.F; }
  static ColorComponentU8 alpha(uint32_t color) { return static_cast<uint8_t>(color & 0xFF); }
};

/**
 * @brief Represents an image with alpha channel (4 bytes per pixel). The pixel format is R8 G8 B8 A8.
 *
 */
class Image {
 public:
  ERAY_DEFAULT_MOVE_AND_COPY_CTOR(Image)
  ERAY_DEFAULT_MOVE_AND_COPY_ASSIGN(Image)

  static Image create(uint32_t width, uint32_t height, ColorU32 color = 0x000000FF);
  static Image create(uint32_t width, uint32_t height, uint32_t bpp, std::vector<ColorU32>&& data);

  enum class LoadError : uint8_t {
    FileDoesNotExist = 0,
    InvalidFile      = 1,
  };
  static util::Result<Image, LoadError> load_from_path(const std::filesystem::path& path);

  bool is_in_bounds(ColorU32 x, ColorU32 y) const;
  void set_pixel(ColorU32 x, ColorU32 y, ColorU32 color);
  void set_pixel_safe(ColorU32 x, ColorU32 y, ColorU32 color);
  ColorU32 pixel(ColorU32 x, ColorU32 y) const;
  void resize(uint32_t new_width, uint32_t new_height, ColorU32 color = 0x000000FF);
  void clear(ColorU32 color);

  size_t width() const { return width_; }
  size_t height() const { return height_; }
  size_t size_in_bytes() const { return width_ * height_ * sizeof(uint32_t); }

  const ColorU32* raw() const { return data_.data(); }
  const ColorComponentU8* raw_bytes() const { return reinterpret_cast<const ColorComponentU8*>(data_.data()); }

 private:
  Image();
  Image(uint32_t width, uint32_t height, ColorU32 color = 0x000000FF);
  Image(uint32_t width, uint32_t height, uint32_t bpp, std::vector<ColorU32>&& data);

  static void swap_endianess(uint32_t* pixel) {
    *pixel = ((*pixel & 0xFF000000) >> 24) | ((*pixel & 0x00FF0000) >> 8) | ((*pixel & 0x0000FF00) << 8) |
             ((*pixel & 0x000000FF) << 24);
  }

  size_t width_;
  size_t height_;
  size_t bpp_ = 4;

  std::vector<ColorU32> data_;
};

}  // namespace eray::res

