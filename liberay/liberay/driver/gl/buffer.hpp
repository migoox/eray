#pragma once

#include <glad/gl.h>

#include <expected>
#include <liberay/driver/gl/gl_error.hpp>
#include <liberay/driver/gl/gl_handle.hpp>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/zstring_view.hpp>
#include <span>
#include <unordered_map>
#include <vector>

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
  ERAY_DEFAULT_MOVE(Buffer)

  GLuint raw_gl_id() const { return id_.get(); }

 protected:
  explicit Buffer(GLuint id);

 protected:
  friend VertexArray;
  BufferHandle id_;
};

template <typename T>
concept CPrimitiveType = (std::is_same_v<T, float> || std::is_same_v<T, const float> || std::is_same_v<T, int> ||
                          std::is_same_v<T, const int>);

/**
 * @brief Represents a buffer, interpreted as a sequence of vertices. Each vertex
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
  VertexBuffer() = delete;
  /**
   * @brief Defines array of vertices layout that is used to in OpenGL vertex buffer.
   *
   */
  class Layout {
   public:
    struct Attribute {
      /**
       * @brief Creates an attribute.
       *
       * @param location refers to a location, that the attribute will be bound to in VAO.
       * @param count measured in number of elements, e.g. if element is of float type, count=3 means 12 bytes.
       * @param bytes_offset represents an offset from the begining of a structure in the array in bytes.
       * @param normalize
       * @param index
       */
      template <CPrimitiveType PrimitiveType>
      static Attribute create(size_t location, size_t count, size_t bytes_offset, bool normalize = false) {
        GLenum type = GL_FLOAT;
        if constexpr (std::is_same_v<PrimitiveType, float>) {
          type = GL_FLOAT;
        } else if constexpr (std::is_same_v<PrimitiveType, int>) {
          type = GL_INT;
        } else {
          static_assert(false, "Unsupported PrimitiveType");
        }

        return {
            .location        = location,               //
            .count           = count,                  //
            .normalize       = normalize,              //
            .bytes_type_size = sizeof(PrimitiveType),  //
            .type            = type,                   //
            .bytes_offset    = bytes_offset,           //
        };
      }

      size_t location;
      size_t count;
      bool normalize;
      size_t bytes_type_size;
      GLenum type;
      size_t bytes_offset;
    };

    template <CPrimitiveType PrimitiveType>
    auto add_attribute(zstring_view name, size_t location, size_t count, bool normalize = false) {
      attribs_.emplace_back(Attribute::create<PrimitiveType>(location, count, current_bytes_offset_, normalize));
      indices_.insert({name, attribs_.size() - 1});
      current_bytes_offset_ += sizeof(PrimitiveType) * count;
    }
    const Attribute& attribute(zstring_view name) const { return attribs_.at(indices_.at(name)); }

    size_t bytes_size() const { return current_bytes_offset_; }

    auto begin() const { return attribs_.begin(); }
    auto end() const { return attribs_.end(); }

   private:
    std::unordered_map<zstring_view, size_t> indices_;
    std::vector<Attribute> attribs_;
    size_t current_bytes_offset_;
  };

  /**
   * @brief Create a vertex buffer with the specified layout.
   *
   * @param layout keys should represent locations
   * @return VertexBuffer
   */
  static VertexBuffer create(Layout&& layout);

  template <CPrimitiveType PrimitiveType>
  void buffer_data(std::span<PrimitiveType> vertices, DataUsage usage) {
    ERAY_GL_CALL(glNamedBufferData(id_.get(), static_cast<GLsizeiptr>(vertices.size() * sizeof(PrimitiveType)),
                                   reinterpret_cast<const void*>(vertices.data()), kDataUsageGLMapper[usage]));
  }

  /**
   * @brief Calls glNamedBufferSubData.
   *
   * @param offset_count is measured in floats not bytes.
   * @param vertices
   */
  void sub_buffer_data(GLuint vert_start_index, const void* vertices, size_t count) {
    ERAY_GL_CALL(glNamedBufferSubData(id_.get(), static_cast<GLintptr>(vert_start_index * layout_.bytes_size()),
                                      static_cast<GLsizeiptr>(count * layout_.bytes_size()), vertices));
  }

  /**
   * @brief Calls glNamedBufferSubData.
   *
   * @param offset_count is measured in floats not bytes.
   * @param vertices
   */
  template <CPrimitiveType PrimitiveType>
  void set_attribute_value(GLuint vert_start_index, zstring_view attr_name, const PrimitiveType* attr_value) {
    const auto& attrib = layout_.attribute(attr_name);
    ERAY_GL_CALL(glNamedBufferSubData(
        id_.get(), static_cast<GLintptr>(vert_start_index * layout_.bytes_size() + attrib.bytes_offset),
        static_cast<GLsizeiptr>(attrib.count * attrib.bytes_type_size), reinterpret_cast<const void*>(attr_value)));
  }

  const Layout& layout() const { return layout_; }

 private:
  VertexBuffer(GLuint id, Layout&& layout) : Buffer(id), layout_(std::move(layout)) {}

 private:
  Layout layout_;
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

}  // namespace eray::driver::gl
