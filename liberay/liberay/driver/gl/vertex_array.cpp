#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/gl_error.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/zstring_view.hpp>

namespace eray::driver::gl {

// -- VertexArray -----------------------------------------------------------------------------------------------------

VertexArray VertexArray::create(VertexBuffer&& vert_buff, ElementBuffer&& ebo_buff) {
  GLuint id = 0;
  glCreateVertexArrays(1, &id);

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

void VertexArray::set_binding_divisor(GLuint divisor) {  // NOLINT
  glVertexArrayBindingDivisor(m_.id, 0, divisor);
  check_gl_errors();
}

VertexArray::VertexArray(VertexArray&& other) noexcept : m_(std::move(other.m_)) { other.m_.id = 0; }

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
  m_          = std::move(other.m_);
  other.m_.id = 0;

  return *this;
}

VertexArray::~VertexArray() { glDeleteVertexArrays(1, &m_.id); }

// -- VertexArrays ----------------------------------------------------------------------------------------------------

VertexArrays VertexArrays::create(std::unordered_map<zstring_view, VertexBuffer>&& vert_buffs,
                                  ElementBuffer&& ebo_buff) {
  GLuint id = 0;
  glCreateVertexArrays(1, &id);

  // Bind EBO to VAO
  glVertexArrayElementBuffer(id, ebo_buff.raw_gl_id());

  std::unordered_map<zstring_view, GLuint> vbos_binding_ind;

  // Apply layouts of VBOs
  GLuint binding_ind = 0;
  for (const auto& [name, vert_buff] : vert_buffs) {
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
      glVertexArrayAttribBinding(id, attrib.index, binding_ind);
    }
    vbos_binding_ind.insert({name, binding_ind});

    glVertexArrayVertexBuffer(id, binding_ind++, vert_buff.raw_gl_id(), 0, vertex_size);
  }

  check_gl_errors();

  return VertexArrays({
      .vbos             = std::move(vert_buffs),
      .vbos_binding_ind = std::move(vbos_binding_ind),
      .ebo              = std::move(ebo_buff),
      .id               = id,
  });
}

void VertexArrays::set_binding_divisor(zstring_view name, GLuint divisor) {  // NOLINT
  glVertexArrayBindingDivisor(m_.id, m_.vbos_binding_ind.at(name), divisor);
  check_gl_errors();
}

VertexArrays::VertexArrays(VertexArrays&& other) noexcept : m_(std::move(other.m_)) { other.m_.id = 0; }

VertexArrays& VertexArrays::operator=(VertexArrays&& other) noexcept {
  m_          = std::move(other.m_);
  other.m_.id = 0;

  return *this;
}

VertexArrays::~VertexArrays() { glDeleteVertexArrays(1, &m_.id); }

}  // namespace eray::driver::gl
