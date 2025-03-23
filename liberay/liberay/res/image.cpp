#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <expected>
#include <filesystem>
#include <liberay/res/image.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/path_utf8.hpp>
#include <vector>

namespace eray::res {

Image::Image(uint32_t width, uint32_t height, ColorU32 color) : width_(width), height_(height) {
  data_ = std::vector<uint32_t>(width_ * height_, color);
}

Image::Image(uint32_t width, uint32_t height, uint32_t bpp, std::vector<ColorU32>&& data)
    : width_(width), height_(height), bpp_(bpp), data_(std::move(data)) {}

namespace {

void swap_endianess(uint32_t* pixel) {
  *pixel = ((*pixel & 0xFF000000) >> 24) | ((*pixel & 0x00FF0000) >> 8) | ((*pixel & 0x0000FF00) << 8) |
           ((*pixel & 0x000000FF) << 24);
}

}  // namespace

std::expected<Image, Image::LoadError> Image::load_from_path(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
    util::Logger::err(R"(Provided image file with path "{}" does not exist or is not a file.)", path.string());
    return std::unexpected(LoadError::FileDoesNotExist);
  }

  stbi_set_flip_vertically_on_load(1);
  int width  = 0;
  int height = 0;
  int bpp    = 0;

  auto* buff = reinterpret_cast<uint32_t*>(stbi_load(util::path_to_utf8str(path).c_str(), &width, &height, &bpp, 4));

  for (int i = 0; i < width * height; ++i) {
    swap_endianess(&buff[i]);
  }

  if (!buff) {
    util::Logger::err("Could not load the image. The file is invalid.");
    return std::unexpected(LoadError::InvalidFile);
  }

  auto data = std::vector<ColorU32>();
  data.insert(data.end(), &buff[0], &buff[width * height]);
  stbi_image_free(reinterpret_cast<void*>(buff));
  return Image(static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(bpp),
               std::move(data));
}

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
