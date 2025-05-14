#pragma once
#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/zstring_view.hpp>
#include <unordered_map>

namespace eray::driver::gl {

class VertexArray {
 public:
  ERAY_DISABLE_COPY(VertexArray)

  VertexArray(VertexArray&& other) noexcept;
  VertexArray& operator=(VertexArray&& other) noexcept;

  static VertexArray create(VertexBuffer&& vert_buff, ElementBuffer&& ebo_buff);
  ~VertexArray();

  /**
   * @brief Binds the VertexArray. It's required only before calling the draw as the
   * class uses DSA (Direct State Access).
   *
   */
  void bind() const { ERAY_GL_CALL(glBindVertexArray(m_.id)); }

  static void unbind() { ERAY_GL_CALL(glBindVertexArray(0)); }

  void set_binding_divisor(GLuint divisor);

  const VertexBuffer& vbo() const { return m_.vbo; }
  VertexBuffer& vbo() { return m_.vbo; }

  const ElementBuffer& ebo() const { return m_.ebo; }
  ElementBuffer& ebo() { return m_.ebo; }

 private:
  struct Members {
    VertexBuffer vbo;
    ElementBuffer ebo;
    GLuint id;
  };
  explicit VertexArray(Members&& m) : m_(std::move(m)) {}

 private:
  Members m_;
};

class VertexArrays {
 public:
  ERAY_DISABLE_COPY(VertexArrays)

  VertexArrays(VertexArrays&& other) noexcept;
  VertexArrays& operator=(VertexArrays&& other) noexcept;

  static VertexArrays create(std::unordered_map<zstring_view, VertexBuffer>&& vert_buff, ElementBuffer&& ebo_buff);
  ~VertexArrays();

  /**
   * @brief Binds the VertexArrays. It's required only before calling the draw as the
   * class uses DSA (Direct State Access).
   *
   */
  void bind() const { ERAY_GL_CALL(glBindVertexArray(m_.id)); }

  static void unbind() { ERAY_GL_CALL(glBindVertexArray(0)); }

  void set_binding_divisor(zstring_view name, GLuint divisor);

  const VertexBuffer& vbo(zstring_view name) const { return m_.vbos.at(name); }
  VertexBuffer& vbo(zstring_view name) { return m_.vbos.at(name); }

  const ElementBuffer& ebo() const { return m_.ebo; }
  ElementBuffer& ebo() { return m_.ebo; }

 private:
  struct Members {
    std::unordered_map<zstring_view, VertexBuffer> vbos;
    std::unordered_map<zstring_view, GLuint> vbos_binding_ind;
    ElementBuffer ebo;
    GLuint id;
  };
  explicit VertexArrays(Members&& m) : m_(std::move(m)) {}

 private:
  Members m_;
};

}  // namespace eray::driver::gl
