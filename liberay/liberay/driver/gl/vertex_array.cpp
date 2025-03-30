#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/vertex_array.hpp>

#include "liberay/driver/gl/gl_error.hpp"

namespace eray::driver::gl {

VertexArray VertexArray::create(VertexBuffer&& vert_buff, ElementBuffer&& ebo_buff) {
  GLuint id = 0;
  glCreateVertexArrays(1, &id);

  // Bind VBO to VAO

  // Bind EBO to VAO
  glVertexArrayElementBuffer(id, ebo_buff.id_);

  // Apply layouts of VBO
  GLsizei vertex_size = 0;

  for (const auto& attrib : vert_buff.layout()) {
    glEnableVertexArrayAttrib(id, attrib.index);
    glVertexArrayAttribFormat(id,                                     //
                              attrib.index,                           //
                              static_cast<GLint>(attrib.count),       //
                              GL_FLOAT,                               //
                              attrib.normalize ? GL_TRUE : GL_FALSE,  //
                              vertex_size);                           //

    vertex_size += static_cast<GLint>(sizeof(float) * attrib.count);
    glVertexArrayAttribBinding(id, attrib.index, 0);
  }

  glVertexArrayVertexBuffer(id, 0, vert_buff.id_, 0, vertex_size);

  check_gl_errors();

  return VertexArray({
      .vbo = std::move(vert_buff),
      .ebo = std::move(ebo_buff),
      .id  = id,
  });
}

void VertexArray::set_binding_divisor(GLuint binding_index, GLuint divisor) {  // NOLINT
  glVertexArrayBindingDivisor(m_.id, binding_index, divisor);
  check_gl_errors();
}

VertexArray::VertexArray(VertexArray&& other) noexcept : m_(std::move(other.m_)) { other.m_.id = 0; }

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
  m_          = std::move(other.m_);
  other.m_.id = 0;

  return *this;
}

}  // namespace eray::driver::gl
