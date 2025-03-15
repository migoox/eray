#pragma once

#include <glad/gl.h>

#include <cstddef>
#include <liberay/util/ruleof.hpp>

namespace eray::driver::gl {

class Framebuffer {
 public:
  Framebuffer(size_t width, size_t height);
  virtual ~Framebuffer();

  ERAY_DISABLE_COPY(Framebuffer)

  Framebuffer(Framebuffer&& other) noexcept;
  Framebuffer& operator=(Framebuffer&& other) = delete;

  void bind() const;
  void unbind() const;  // remember to glVieport after

  virtual void clear()                             = 0;
  virtual void resize(size_t width, size_t height) = 0;

  size_t width() const { return width_; }
  size_t height() const { return height_; }

 protected:
  void start_init() const;
  void end_init() const;

 protected:
  size_t width_, height_;
  GLuint framebuffer_id_;
};

class ImageFramebuffer : public Framebuffer {
 public:
  ImageFramebuffer(size_t width, size_t height);
  virtual ~ImageFramebuffer();

  ImageFramebuffer(const ImageFramebuffer& other) = delete;
  ImageFramebuffer(ImageFramebuffer&& other) noexcept;
  ImageFramebuffer& operator=(const ImageFramebuffer& other) = delete;
  ImageFramebuffer& operator=(ImageFramebuffer&& other)      = delete;

  void clear() override;
  void resize(size_t width, size_t height) override;

  GLuint color_texture() const { return color_attachment_texture_; }

 private:
  GLuint color_attachment_texture_;
};

}  // namespace eray::driver::gl
