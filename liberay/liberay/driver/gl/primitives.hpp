#pragma once
#include <glad/gl.h>

#include <cstdint>
#include <liberay/util/enum_mapper.hpp>

namespace eray::driver::gl {

enum class Primitive : std::uint8_t {
  Points        = 0,
  Lines         = 1,
  LineStrip     = 2,
  LineLoop      = 3,
  Triangles     = 4,
  TriangleStrip = 5,
  TriangleFan   = 6,
  _Count        = 7  // NOLINT
};

constexpr auto kPrimitiveGLCode = util::EnumMapper<Primitive, GLenum>({{Primitive::Points, GL_POINTS},
                                                                       {Primitive::Lines, GL_LINES},
                                                                       {Primitive::LineStrip, GL_LINE_STRIP},
                                                                       {Primitive::LineLoop, GL_LINE_LOOP},
                                                                       {Primitive::Triangles, GL_TRIANGLES},
                                                                       {Primitive::TriangleStrip, GL_TRIANGLE_STRIP},
                                                                       {Primitive::TriangleFan, GL_TRIANGLE_FAN}});

}  // namespace eray::driver::gl
