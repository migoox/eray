#include <glad/gl.h>

#include <liberay/driver/gl/framebuffer.hpp>
#include <liberay/util/logger.hpp>
#include <unordered_set>

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

ViewportFramebuffer::ViewportFramebuffer(size_t width, size_t height)
    : Framebuffer(width, height),
      color_attachment_texture_(0),
      mouse_pick_attachment_texture_(0),
      depth_renderbuffer_(0) {
  start_init();

  // Setup color attachment
  glGenTextures(1, &color_attachment_texture_);
  glBindTexture(GL_TEXTURE_2D, color_attachment_texture_);
  prepare_texture(GL_RGBA, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_attachment_texture_, 0);

  // Setup depth attachment (without stencil)
  glGenRenderbuffers(1, &depth_renderbuffer_);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(width),
                        static_cast<GLsizei>(height));
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_renderbuffer_);

  // Setup mouse pick attachment
  glGenTextures(1, &mouse_pick_attachment_texture_);
  glBindTexture(GL_TEXTURE_2D, mouse_pick_attachment_texture_);
  prepare_texture(GL_RED_INTEGER, GL_R32I, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mouse_pick_attachment_texture_, 0);

  end_init();
}

ViewportFramebuffer::~ViewportFramebuffer() {
  if (color_attachment_texture_ == 0) {
    return;
  }

  glDeleteRenderbuffers(1, &depth_renderbuffer_);
  glDeleteTextures(1, &color_attachment_texture_);
  glDeleteTextures(1, &mouse_pick_attachment_texture_);
}

ViewportFramebuffer::ViewportFramebuffer(ViewportFramebuffer&& other) noexcept
    : Framebuffer(std::move(other)),
      color_attachment_texture_(other.color_attachment_texture_),
      mouse_pick_attachment_texture_(other.mouse_pick_attachment_texture_),
      depth_renderbuffer_(other.depth_renderbuffer_) {
  other.color_attachment_texture_      = 0;
  other.mouse_pick_attachment_texture_ = 0;
  other.depth_renderbuffer_            = 0;
}

void ViewportFramebuffer::clear_pick_render() const {
  static constexpr int kClear = -1;
  glClearTexImage(mouse_pick_attachment_texture_, 0, GL_RED_INTEGER, GL_INT, &kClear);
}

void ViewportFramebuffer::begin_pick_render() const {
  static constexpr std::array<GLenum, 2> kAttachments = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(static_cast<GLsizei>(kAttachments.size()), kAttachments.data());
}

void ViewportFramebuffer::begin_pick_render_only() const {
  static constexpr std::array<GLenum, 2> kAttachments = {GL_NONE, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(static_cast<GLsizei>(kAttachments.size()), kAttachments.data());
}

void ViewportFramebuffer::end_pick_render() const {  // NOLINT
  static constexpr std::array<GLenum, 2> kAttachments = {GL_COLOR_ATTACHMENT0, GL_NONE};
  glDrawBuffers(static_cast<GLsizei>(kAttachments.size()), kAttachments.data());
}

void ViewportFramebuffer::clear() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

void ViewportFramebuffer::resize(size_t width, size_t height) {
  width_  = width;
  height_ = height;

  // Resize color attachment
  glBindTexture(GL_TEXTURE_2D, color_attachment_texture_);
  prepare_texture(GL_RGBA, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

  // Resize depth attachment
  glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(width),
                        static_cast<GLsizei>(height));

  // Resize mouse pick attachment
  glBindTexture(GL_TEXTURE_2D, mouse_pick_attachment_texture_);
  prepare_texture(GL_RED_INTEGER, GL_R32I, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

int ViewportFramebuffer::sample_mouse_pick(const size_t x, const size_t y) const {  // NOLINT
  glReadBuffer(GL_COLOR_ATTACHMENT1);
  int pixel = -1;
  glReadPixels(static_cast<GLint>(x), static_cast<GLint>(height_ - y), 1, 1, GL_RED_INTEGER, GL_INT, &pixel);
  return pixel;
}

std::unordered_set<int> ViewportFramebuffer::sample_mouse_pick_box(const size_t x, const size_t y, const size_t width,
                                                                   const size_t height) const {  // NOLINT
  auto glwidth  = width;
  auto glheight = height;
  if (width + x > width_) {
    glwidth = width_ - x;
  }
  if (height + y > height_) {
    glheight = height_ - y;
  }

  glReadBuffer(GL_COLOR_ATTACHMENT1);
  auto pixels = std::vector<int>(width * height);

  auto gly = static_cast<GLint>(height_ - y - height);
  if (gly < 1) {
    return {};
  }

  glReadPixels(static_cast<GLint>(x), gly, glwidth, glheight, GL_RED_INTEGER, GL_INT, pixels.data());

  std::unordered_set<int> result;
  for (auto id : pixels) {
    if (id == -1) {
      continue;
    }
    result.insert(id);
  }
  return result;
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
}

ImageFramebuffer::~ImageFramebuffer() {
  if (color_attachment_texture_ == 0) {
    return;
  }
  glDeleteTextures(1, &color_attachment_texture_);
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
