#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <expected>
#include <filesystem>
#include <liberay/res/error.hpp>
#include <liberay/res/file.hpp>
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

Image Image::create(uint32_t width, uint32_t height, ColorU32 color) { return Image(width, height, color); }

Image Image::create(uint32_t width, uint32_t height, uint32_t bpp, std::vector<ColorU32>&& data) {
  return Image(width, height, bpp, std::move(data));
}

util::Result<Image, FileError> Image::load_from_path(const std::filesystem::path& path) {
  auto validation_result = validate_file(path);
  if (!validation_result) {
    return std::unexpected(validation_result.error());
  }

  stbi_set_flip_vertically_on_load(1);
  int width  = 0;
  int height = 0;
  int bpp    = 0;

  auto* buff = reinterpret_cast<uint32_t*>(
      stbi_load(util::path_to_utf8str(path).c_str(), &width, &height, &bpp, STBI_rgb_alpha));

  if (!buff) {
    util::Logger::err("Could not load the image. The file is invalid.");
    return std::unexpected(FileError{
        .path = path,
        .msg  = "stbi_load result is NULL",
        .code = FileErrorCode::ReadFailure,
    });
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

uint32_t Image::mip_levels() const {
  return static_cast<uint32_t>(std::floor(std::log2(std::max(width_, height_)))) + 1;
}

}  // namespace eray::res
