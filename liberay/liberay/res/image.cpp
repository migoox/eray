#include <expected>
#include <liberay/res/image.hpp>
#include <vector>


namespace eray::res {

Image::Image(uint32_t width, uint32_t height, ColorU32 color) : width_(width), height_(height) {
  data_ = std::vector<uint32_t>(width_ * height_, color);
}

Image::Image(ColorU32 width, ColorU32 height, uint32_t bpp, std::vector<ColorU32>&& data)
    : width_(width), height_(height), bpp_(bpp), data_(std::move(data)) {}

void Image::clear(uint32_t color) {
  for (auto& pixel : data_) {
    pixel = color;
  }
}

void Image::set_pixel_safe(uint32_t x, uint32_t y, uint32_t color) {
  if (!is_in_bounds(x, y)) {
    return;
  }

  data_[x + y * width_] = color;
}

void Image::set_pixel(uint32_t x, uint32_t y, uint32_t color) { data_[x + y * width_] = color; }

void Image::resize(uint32_t new_width, uint32_t new_height, uint32_t color) {
  data_.resize(new_width * new_height, color);
  width_  = new_width;
  height_ = new_height;
}

bool Image::is_in_bounds(uint32_t x, uint32_t y) const { return x < width_ && y < height_; }

uint32_t Image::pixel(uint32_t x, uint32_t y) const { return data_[x + y * width_]; }

}  // namespace eray::res
