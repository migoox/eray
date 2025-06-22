#include <glad/gl.h>

#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/gl_error.hpp>
#include <liberay/driver/gl/vertex_array.hpp>
#include <liberay/util/zstring_view.hpp>

#include "liberay/driver/gl/gl_handle.hpp"

namespace eray::driver::gl {

// -- VertexArray -----------------------------------------------------------------------------------------------------

VertexArray VertexArray::create(VertexBuffer&& vert_buff, ElementBuffer&& ebo_buff) {
  GLuint id = 0;
  ERAY_GL_CALL(glCreateVertexArrays(1, &id));

  // Bind EBO to VAO
  ERAY_GL_CALL(glVertexArrayElementBuffer(id, ebo_buff.raw_gl_id()));

  // Apply layouts of VBO
  GLsizei vertex_size = 0;

  for (const auto& attrib : vert_buff.layout()) {
    ERAY_GL_CALL(glEnableVertexArrayAttrib(id, attrib.location));
    ERAY_GL_CALL(glVertexArrayAttribFormat(id,                                     //
                                           attrib.location,                        //
                                           static_cast<GLint>(attrib.count),       //
                                           attrib.type,                            //
                                           attrib.normalize ? GL_TRUE : GL_FALSE,  //
                                           vertex_size));                          //

    vertex_size += static_cast<GLint>(sizeof(float) * attrib.count);
    ERAY_GL_CALL(glVertexArrayAttribBinding(id, attrib.location, 0));
  }

  ERAY_GL_CALL(glVertexArrayVertexBuffer(id, 0, vert_buff.raw_gl_id(), 0, vertex_size));

  return VertexArray({
      .vbo = std::move(vert_buff),
      .ebo = std::move(ebo_buff),
      .id  = id,
  });
}

void VertexArray::set_binding_divisor(GLuint divisor) {  // NOLINT
  ERAY_GL_CALL(glVertexArrayBindingDivisor(m_.id, 0, divisor));
}

VertexArray::VertexArray(VertexArray&& other) noexcept : m_(std::move(other.m_)) { other.m_.id = 0; }

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
  m_          = std::move(other.m_);
  other.m_.id = 0;

  return *this;
}

VertexArray::~VertexArray() { ERAY_GL_CALL(glDeleteVertexArrays(1, &m_.id)); }

// -- SimpleVertexArray ------------------------------------------------------------------------------------------------

SimpleVertexArray SimpleVertexArray::create(VertexBuffer&& vert_buff) {
  GLuint id = 0;
  ERAY_GL_CALL(glCreateVertexArrays(1, &id));

  GLsizei vertex_size = 0;
  for (const auto& attrib : vert_buff.layout()) {
    ERAY_GL_CALL(glEnableVertexArrayAttrib(id, attrib.location));
    ERAY_GL_CALL(glVertexArrayAttribFormat(id,                                     //
                                           attrib.location,                        //
                                           static_cast<GLint>(attrib.count),       //
                                           attrib.type,                            //
                                           attrib.normalize ? GL_TRUE : GL_FALSE,  //
                                           vertex_size));                          //

    vertex_size += static_cast<GLint>(sizeof(float) * attrib.count);
    ERAY_GL_CALL(glVertexArrayAttribBinding(id, attrib.location, 0));
  }

  ERAY_GL_CALL(glVertexArrayVertexBuffer(id, 0, vert_buff.raw_gl_id(), 0, vertex_size));

  return SimpleVertexArray({
      .vbo = std::move(vert_buff),
      .id  = VertexArrayHandle(id),
  });
}

void SimpleVertexArray::set_binding_divisor(GLuint divisor) {  // NOLINT
  ERAY_GL_CALL(glVertexArrayBindingDivisor(m_.id.get(), 0, divisor));
}

// -- VertexArrays -----------------------------------------------------------------------------------------------------

VertexArrays VertexArrays::create(std::unordered_map<zstring_view, VertexBuffer>&& vert_buffs,
                                  ElementBuffer&& ebo_buff) {
  GLuint id = 0;
  ERAY_GL_CALL(glCreateVertexArrays(1, &id));

  // Bind EBO to VAO
  ERAY_GL_CALL(glVertexArrayElementBuffer(id, ebo_buff.raw_gl_id()));

  std::unordered_map<zstring_view, GLuint> vbos_binding_ind;

  // Apply layouts of VBOs
  GLuint binding_ind = 0;
  for (const auto& [name, vert_buff] : vert_buffs) {
    GLsizei vertex_size = 0;
    for (const auto& attrib : vert_buff.layout()) {
      ERAY_GL_CALL(glEnableVertexArrayAttrib(id, attrib.location));
      ERAY_GL_CALL(glVertexArrayAttribFormat(id,                                     //
                                             attrib.location,                        //
                                             static_cast<GLint>(attrib.count),       //
                                             GL_FLOAT,                               //
                                             attrib.normalize ? GL_TRUE : GL_FALSE,  //
                                             vertex_size));                          //

      vertex_size += static_cast<GLint>(sizeof(float) * attrib.count);
      ERAY_GL_CALL(glVertexArrayAttribBinding(id, attrib.location, binding_ind));
    }
    vbos_binding_ind.insert({name, binding_ind});

    ERAY_GL_CALL(glVertexArrayVertexBuffer(id, binding_ind++, vert_buff.raw_gl_id(), 0, vertex_size));
  }

  return VertexArrays({
      .vbos             = std::move(vert_buffs),
      .vbos_binding_ind = std::move(vbos_binding_ind),
      .ebo              = std::move(ebo_buff),
      .id               = id,
  });
}

void VertexArrays::set_binding_divisor(zstring_view name, GLuint divisor) {  // NOLINT
  ERAY_GL_CALL(glVertexArrayBindingDivisor(m_.id, m_.vbos_binding_ind.at(name), divisor));
}

VertexArrays::VertexArrays(VertexArrays&& other) noexcept : m_(std::move(other.m_)) { other.m_.id = 0; }

VertexArrays& VertexArrays::operator=(VertexArrays&& other) noexcept {
  m_          = std::move(other.m_);
  other.m_.id = 0;

  return *this;
}

VertexArrays::~VertexArrays() { ERAY_GL_CALL(glDeleteVertexArrays(1, &m_.id)); }

}  // namespace eray::driver::gl
