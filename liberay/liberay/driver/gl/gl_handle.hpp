#include <glad/gl.h>

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

  ERAY_DISABLE_COPY(GLObjectHandle)

  GLObjectHandle(GLObjectHandle&& other) noexcept : id_(other.id_) { other.id_ = 0; }
  GLObjectHandle& operator=(GLObjectHandle&& other) noexcept {
    id_       = other.id_;
    other.id_ = 0;
  }

  GLuint get() { return id_; }
  GLuint release() {
    GLuint id = id_;
    id_       = 0;
    return id;
  }

 private:
  GLuint id_;
};

struct ShaderTag {};
using ShaderHandle = GLObjectHandle<ShaderTag, [](auto id) { glDeleteShader(id); }>;

struct ShaderProgramTag {};
using ShaderProgramHandle = GLObjectHandle<ShaderProgramTag, [](auto id) { glDeleteProgram(id); }>;

struct TextureTag {};
using TextureHandle = GLObjectHandle<ShaderProgramTag, [](auto id) { glDeleteTextures(1, &id); }>;

struct VertexArrayTag {};
using VertexArrayHandle = GLObjectHandle<VertexArrayTag, [](auto id) { glDeleteVertexArrays(1, &id); }>;

struct BufferTag {};
using BufferHandle = GLObjectHandle<BufferTag, [](auto id) { glDeleteBuffers(1, &id); }>;

}  // namespace eray::driver::gl
