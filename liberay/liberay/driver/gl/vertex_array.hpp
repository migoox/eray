#pragma once
#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/util/ruleof.hpp>

namespace eray::driver::gl {

class VertexArray {
 public:
  ERAY_DISABLE_COPY(VertexArray)

  VertexArray(VertexArray&& other) noexcept;
  VertexArray& operator=(VertexArray&& other) noexcept;

  static VertexArray create(VertexBuffer&& vert_buff, ElementBuffer&& ebo_buff);

  /**
   * @brief Binds the VertexArray. It's required only before calling the draw.
   *
   */
  void bind() const { glBindVertexArray(m_.id); }

  static void unbind() { glBindVertexArray(0); }

  const VertexBuffer& vbo() const { return m_.vbo; }

  const ElementBuffer& ebo() const { return m_.ebo; }

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

}  // namespace eray::driver::gl
