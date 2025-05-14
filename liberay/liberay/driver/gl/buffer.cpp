#include <glad/gl.h>

#include <cstdint>
#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/gl_error.hpp>
#include <liberay/util/zstring_view.hpp>

namespace eray::driver::gl {

// -- Buffer ----------------------------------------------------------------------------------------------------------

Buffer::Buffer(GLuint id) : id_(id) {}

// -- VertexBuffer ----------------------------------------------------------------------------------------------------

VertexBuffer VertexBuffer::create(Layout&& layout) {
  GLuint id = 0;
  ERAY_GL_CALL(glCreateBuffers(1, &id));
  return VertexBuffer(id, std::move(layout));
}

// -- IndexBuffer -----------------------------------------------------------------------------------------------------

ElementBuffer ElementBuffer::create() {
  GLuint id = 0;
  ERAY_GL_CALL(glCreateBuffers(1, &id));
  return ElementBuffer(id);
}

void ElementBuffer::buffer_data(std::span<uint32_t> indices, DataUsage usage) {  // NOLINT
  count_ = indices.size();
  ERAY_GL_CALL(glNamedBufferData(id_.get(), static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                                 reinterpret_cast<const void*>(indices.data()), kDataUsageGLMapper[usage]));
}

void ElementBuffer::sub_buffer_data(GLuint offset_count, std::span<uint32_t> indices) {
  ERAY_GL_CALL(glNamedBufferSubData(id_.get(), static_cast<GLintptr>(offset_count * sizeof(uint32_t)),
                                    static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                                    reinterpret_cast<const void*>(indices.data())));
}

}  // namespace eray::driver::gl
