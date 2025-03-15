#include <glad/gl.h>

#include <liberay/driver/gl/framebuffer.hpp>
#include <liberay/util/logger.hpp>

namespace eray::driver::gl {

namespace {

void prepare_texture(GLenum format, GLint internal_format, GLsizei width, GLsizei height) {
  glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);
}

}  // namespace

Framebuffer::Framebuffer(size_t width, size_t height) : width_(width), height_(height), framebuffer_id_(0) {
  glCreateFramebuffers(1, &framebuffer_id_);
}

Framebuffer::~Framebuffer() { glDeleteFramebuffers(1, &framebuffer_id_); }

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : width_(other.width_), height_(other.height_), framebuffer_id_(other.framebuffer_id_) {
  other.framebuffer_id_ = 0;
  other.width_          = 0;
  other.height_         = 0;
}

void Framebuffer::bind() const {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
  glViewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
}

void Framebuffer::unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }  // NOLINT

void Framebuffer::start_init() const { glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_); }

void Framebuffer::end_init() const {  // NOLINT
  // Verify framebuffer creation
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    // TODO::ERROR
    std::terminate();
  }

  // Bind default framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ImageFramebuffer::ImageFramebuffer(size_t width, size_t height)
    : Framebuffer(width, height), color_attachment_texture_(0) {
  start_init();

  // Setup color attachment
  glGenTextures(1, &color_attachment_texture_);
  glBindTexture(GL_TEXTURE_2D, color_attachment_texture_);
  prepare_texture(GL_RGBA, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_attachment_texture_, 0);

  end_init();

  util::Logger::info("Created new image framebuffer with id {}", color_attachment_texture_);
}

ImageFramebuffer::~ImageFramebuffer() {
  if (color_attachment_texture_ == 0) {
    return;
  }
  glDeleteTextures(1, &color_attachment_texture_);
  util::Logger::info("Deleted image framebuffer with id {}", framebuffer_id_);
}

ImageFramebuffer::ImageFramebuffer(ImageFramebuffer&& other) noexcept
    : Framebuffer(std::move(other)), color_attachment_texture_(other.color_attachment_texture_) {
  other.color_attachment_texture_ = 0;
}

void ImageFramebuffer::clear() { glClear(GL_COLOR_BUFFER_BIT); }

void ImageFramebuffer::resize(size_t width, size_t height) {
  width_  = width;
  height_ = height;

  // Resize color attachment
  glBindTexture(GL_TEXTURE_2D, color_attachment_texture_);
  prepare_texture(GL_RGBA, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

}  // namespace eray::driver::gl
