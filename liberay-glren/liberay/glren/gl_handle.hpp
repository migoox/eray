#pragma once
#include <glad/gl.h>

#include <liberay/glren/gl_error.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/ruleof.hpp>

namespace eray::driver::gl {

template <typename Tag, void (*DeleteFunc)(GLuint)>
class GLObjectHandle {
 public:
  GLObjectHandle() = default;
  explicit GLObjectHandle(GLuint id) : id_(id) {}
  ~GLObjectHandle() {
    if (id_ != 0) {
      DeleteFunc(id_);
    }
  }

  ERAY_DELETE_COPY(GLObjectHandle)

  GLObjectHandle(GLObjectHandle&& other) noexcept : id_(other.id_) { other.id_ = 0; }
  GLObjectHandle& operator=(GLObjectHandle&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    if (id_ != 0) {
      DeleteFunc(id_);
    }

    id_       = other.id_;
    other.id_ = 0;

    return *this;
  }

  GLuint get() const { return id_; }

  GLuint release() {
    GLuint id = id_;
    id_       = 0;
    return id;
  }

 private:
  GLuint id_ = 0;
};

struct ShaderTag {};
using ShaderHandle = GLObjectHandle<ShaderTag, [](auto id) {
  ERAY_GL_CALL(glDeleteShader(id));
  eray::util::Logger::info("Deleted OpenGL shader with id {}", id);
}>;

struct ShaderProgramTag {};
using ShaderProgramHandle = GLObjectHandle<ShaderProgramTag, [](auto id) {
  ERAY_GL_CALL(glDeleteProgram(id));
  eray::util::Logger::info("Deleted OpenGL shader program with id {}", id);
}>;

struct TextureTag {};
using TextureHandle = GLObjectHandle<TextureTag, [](auto id) {
  ERAY_GL_CALL(glDeleteTextures(1, &id));
  eray::util::Logger::info("Deleted OpenGL texture with id {}", id);
}>;

struct VertexArrayTag {};
using VertexArrayHandle = GLObjectHandle<VertexArrayTag, [](auto id) {
  ERAY_GL_CALL(glDeleteVertexArrays(1, &id));
  eray::util::Logger::info("Deleted OpenGL vertex array with id {}", id);
}>;

struct BufferTag {};
using BufferHandle = GLObjectHandle<BufferTag, [](auto id) {
  ERAY_GL_CALL(glDeleteBuffers(1, &id));
  eray::util::Logger::info("Deleted OpenGL buffer with id {}", id);
}>;

}  // namespace eray::driver::gl
