#include <liberay/driver/gl/buffer.hpp>
#include <liberay/driver/gl/gl_error.hpp>

namespace eray::driver::gl {

// -- Buffer ----------------------------------------------------------------------------------------------------------

Buffer::Buffer(GLuint id) : id_(id) {}

Buffer::Buffer(Buffer&& other) noexcept : id_(other.id_) { other.id_ = 0; }

Buffer& Buffer::operator=(Buffer&& other) noexcept {
  id_       = other.id_;
  other.id_ = 0;
  return *this;
}

Buffer::~Buffer() { glDeleteBuffers(1, &id_); }

// -- VertexBuffer ----------------------------------------------------------------------------------------------------

VertexBuffer VertexBuffer::create(std::vector<VertexBuffer::Attribute>&& layout) {
  GLuint id = 0;
  glCreateBuffers(1, &id);
  return VertexBuffer(id, std::move(layout));
}

void VertexBuffer::buffer_data(std::span<float> vertices, DataUsage usage) {
  glNamedBufferData(id_, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                    reinterpret_cast<const void*>(vertices.data()), kDataUsageGLMapper[usage]);
  //   check_gl_errors();
}

// -- IndexBuffer -----------------------------------------------------------------------------------------------------

ElementBuffer ElementBuffer::create() {
  GLuint id = 0;
  glCreateBuffers(1, &id);
  return ElementBuffer(id);
}

void ElementBuffer::buffer_data(std::span<uint32_t> data, DataUsage usage) {  // NOLINT
  count_ = data.size();
  glNamedBufferData(id_, static_cast<GLsizeiptr>(data.size() * sizeof(uint32_t)),
                    reinterpret_cast<const void*>(data.data()), kDataUsageGLMapper[usage]);
  check_gl_errors();
}

// -- PixelBuffer -----------------------------------------------------------------------------------------------------

PixelBuffer PixelBuffer::create() {
  GLuint id = 0;
  glCreateBuffers(1, &id);
  return PixelBuffer(id);
}

void PixelBuffer::buffer_data(std::span<uint32_t> data, DataUsage usage) {  // NOLINT
  glNamedBufferData(id_, static_cast<GLsizeiptr>(data.size() * sizeof(uint32_t)),
                    reinterpret_cast<const void*>(data.data()), kDataUsageGLMapper[usage]);
  check_gl_errors();
}

void PixelBuffer::map_data(  // NOLINT
    const std::function<void(std::span<uint32_t> data)>& data_operator, size_t size) {
  // glMapBuffer() causes sync issue. If GPU is working with this buffer, glMapBuffer() will wait (stall)
  // until the GPU finishes its job. To avoid waiting, the glBufferData() is called with NULL pointer before
  // glMapBuffer(). The previous data of PBO is discarded and glMapBuffer() returns a new allocated
  // pointer immediately even if GPU is still working with the previous data.
  glNamedBufferData(id_, static_cast<GLsizeiptr>(size), nullptr, GL_STREAM_DRAW);
  check_gl_errors();
  auto* ptr = reinterpret_cast<uint32_t*>(glMapNamedBuffer(id_, GL_WRITE_ONLY));
  data_operator(std::span<uint32_t>(ptr, size));
  glUnmapNamedBuffer(id_);
}

}  // namespace eray::driver::gl
