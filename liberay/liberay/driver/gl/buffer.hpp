#pragma once

#include <glad/gl.h>

#include <expected>
#include <functional>
#include <liberay/driver/gl/gl_error.hpp>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/ruleof.hpp>
#include <span>

namespace eray::driver::gl {

enum class DataUsage : uint8_t {
  StreamDraw  = 0,
  StreamRead  = 1,
  StreamCopy  = 2,
  StaticDraw  = 3,
  StaticRead  = 4,
  StaticCopy  = 5,
  DynamicDraw = 6,
  DynamicRead = 7,
  DynamicCopy = 8,
  _Count      = 9  // NOLINT
};

constexpr auto kDataUsageGLMapper = util::EnumMapper<DataUsage, GLenum>({
    {DataUsage::StreamDraw, GL_STREAM_DRAW},
    {DataUsage::StreamRead, GL_STREAM_READ},
    {DataUsage::StreamCopy, GL_STREAM_COPY},
    {DataUsage::StaticDraw, GL_STATIC_DRAW},
    {DataUsage::StaticRead, GL_STATIC_READ},
    {DataUsage::StaticCopy, GL_STATIC_COPY},
    {DataUsage::DynamicDraw, GL_DYNAMIC_DRAW},
    {DataUsage::DynamicRead, GL_DYNAMIC_READ},
    {DataUsage::DynamicCopy, GL_DYNAMIC_COPY},
});

class VertexArray;

class Buffer {
 public:
  ERAY_DISABLE_COPY(Buffer)

  Buffer(Buffer&& other) noexcept;
  Buffer& operator=(Buffer&& other) noexcept;
  ~Buffer();

  GLuint raw_gl_id() const { return id_; }

 protected:
  explicit Buffer(GLuint id);

 protected:
  friend VertexArray;
  GLuint id_;
};

template <typename T>
concept CPrimitiveType = (std::is_same<T, float>::value || std::is_same<T, int>::value);

/**
 * @brief Represents a buffer of floats, interpreted as a sequence of vertices. Each vertex
 * is composed of attributes.
 *
 * @example A vertex consisting of 2 3d vectors representing position and normal is 6 floats wide. It consists
 * of 2 attributes -- position and normal each 3 floats wide. The first attribute is a position, so the stride
 * of the position is 0 and the stride of a normal is 3.
 *
 * @tparam AttribCount
 */
class VertexBuffer : public Buffer {
 public:
  struct Attribute {
    /**
     * @brief Creates an attribute.
     *
     * @param count measured in number of elements, e.g. if element is of float type, count=3 means 12 bytes.
     * @param normalize
     * @param count
     * @param index
     */
    template <CPrimitiveType PrimitiveType>
    static Attribute create(size_t location, size_t count, bool normalize) {
      GLenum type = GL_FLOAT;
      if constexpr (std::is_same_v<PrimitiveType, float>) {
        type = GL_FLOAT;
      } else if constexpr (std::is_same_v<PrimitiveType, int>) {
        type = GL_INT;
      } else {
        static_assert(false, "Unsupported PrimitiveType");
      }

      return {
          .index     = location,               //
          .count     = count,                  //
          .normalize = normalize,              //
          .type_size = sizeof(PrimitiveType),  //
          .type      = type,                   //
      };
    }

    size_t index;
    size_t count;
    bool normalize;
    size_t type_size;
    GLenum type;
  };

  VertexBuffer() = delete;

  /**
   * @brief Create a vertex buffer with the specified layout.
   *
   * @param layout
   * @return VertexBuffer
   */
  static VertexBuffer create(std::vector<Attribute>&& layout);

  template <CPrimitiveType PrimitiveType>
  void buffer_data(std::span<PrimitiveType> vertices, DataUsage usage) {
    glNamedBufferData(id_, static_cast<GLsizeiptr>(vertices.size() * sizeof(PrimitiveType)),
                      reinterpret_cast<const void*>(vertices.data()), kDataUsageGLMapper[usage]);
    check_gl_errors();
  }

  /**
   * @brief Calls glNamedBufferSubData.
   *
   * @param offset_count is measured in floats not bytes.
   * @param vertices
   */
  template <CPrimitiveType PrimitiveType>
  void sub_buffer_data(GLuint offset_count, std::span<PrimitiveType> vertices) {
    glNamedBufferSubData(id_, static_cast<GLintptr>(offset_count * sizeof(PrimitiveType)),
                         static_cast<GLsizeiptr>(vertices.size() * sizeof(PrimitiveType)),
                         reinterpret_cast<const void*>(vertices.data()));
    check_gl_errors();
  }

  const std::vector<Attribute>& layout() const { return layout_; }

 private:
  VertexBuffer(GLuint id, std::vector<Attribute>&& layout) : Buffer(id), layout_(std::move(layout)) {}

 private:
  std::vector<Attribute> layout_;
};

class ElementBuffer : public Buffer {
 public:
  ElementBuffer() = delete;
  ERAY_DEFAULT_MOVE(ElementBuffer)

  static ElementBuffer create();
  void buffer_data(std::span<uint32_t> indices, DataUsage usage);

  /**
   * @brief Calls glNamedBufferSubData.
   *
   * @param indices is measured in floats not bytes.
   * @param vertices
   */
  void sub_buffer_data(GLuint offset_count, std::span<uint32_t> indices);

  size_t count() const { return count_; }

 private:
  explicit ElementBuffer(GLuint id) : Buffer(id) {}
  size_t count_{};
};

class PixelBuffer : public Buffer {
 public:
  PixelBuffer() = delete;

  static PixelBuffer create();
  void buffer_data(std::span<uint32_t> data, DataUsage usage);
  void map_data(const std::function<void(std::span<uint32_t> data)>& data_operator, size_t size);

 private:
  explicit PixelBuffer(GLuint id) : Buffer(id) {}
};

}  // namespace eray::driver::gl
