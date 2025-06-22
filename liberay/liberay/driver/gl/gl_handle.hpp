#pragma once
#include <glad/gl.h>

#include <liberay/driver/gl/gl_error.hpp>
#include <liberay/util/ruleof.hpp>

namespace eray::driver::gl {

template <void (*DeleteFunc)(GLuint)>
struct GLDeleter {
  void operator()(GLuint id) const {
    if (id != 0) {
      DeleteFunc(id);
    }
  }
};

template <typename Tag, void (*DeleteFunc)(GLuint)>
class GLObjectHandle {
 public:
  GLObjectHandle() = default;
  explicit GLObjectHandle(GLuint id) : id_(id) {}

  ERAY_DELETE_COPY(GLObjectHandle)

  GLObjectHandle(GLObjectHandle&& other) noexcept : id_(other.id_) { other.id_ = 0; }
  GLObjectHandle& operator=(GLObjectHandle&& other) noexcept {
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
using ShaderHandle = GLObjectHandle<ShaderTag, [](auto id) { ERAY_GL_CALL(glDeleteShader(id)); }>;

struct ShaderProgramTag {};
using ShaderProgramHandle = GLObjectHandle<ShaderProgramTag, [](auto id) { ERAY_GL_CALL(glDeleteProgram(id)); }>;

struct TextureTag {};
using TextureHandle = GLObjectHandle<ShaderProgramTag, [](auto id) { ERAY_GL_CALL(glDeleteTextures(1, &id)); }>;

struct VertexArrayTag {};
using VertexArrayHandle = GLObjectHandle<VertexArrayTag, [](auto id) { ERAY_GL_CALL(glDeleteVertexArrays(1, &id)); }>;

struct BufferTag {};
using BufferHandle = GLObjectHandle<BufferTag, [](auto id) { ERAY_GL_CALL(glDeleteBuffers(1, &id)); }>;

}  // namespace eray::driver::gl
