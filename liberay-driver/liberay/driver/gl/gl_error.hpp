#pragma once
#include <glad/gl.h>

#include <liberay/util/panic.hpp>
#include <liberay/util/platform.hpp>

namespace eray::driver::gl {

inline void check_gl_errors() {
  GLenum err = 0;
  while ((err = glGetError()) != GL_NO_ERROR) {
    switch (err) {
      case GL_INVALID_ENUM:
        util::panic("OpenGL invalid enum: An unacceptable value was specified for an enumerated argument.");
        break;
      case GL_INVALID_VALUE:
        util::panic("OpenGL invalid value: A numeric argument was out of range.");
        break;
      case GL_INVALID_OPERATION:
        util::panic("OpenGL invalid operation: The specified operation is not allowed in the current state.");
        break;
      case GL_STACK_OVERFLOW:
        util::panic("OpenGL stack overflow: A stack pushing operation caused a stack overflow.");
        break;
      case GL_STACK_UNDERFLOW:
        util::panic(
            "OpenGL stack underflow: A stack popping operation occurred while the stack was at its lowest point.");
        break;
      case GL_OUT_OF_MEMORY:
        util::panic("OpenGL out of memory: There is not enough memory left to execute the command.");
        break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        util::panic("OpenGL invalid framebuffer operation: The framebuffer object is not complete.");
        break;
      case GL_CONTEXT_LOST:
        util::panic("OpenGL context lost: The OpenGL context has been lost, possibly due to a graphics driver crash.");
        break;
      case GL_TABLE_TOO_LARGE_EXT:
        util::panic("OpenGL table too large: The specified table exceeds the implementation's maximum supported size.");
        break;
      default:
        util::panic("OpenGL unknown error: An unrecognized error occurred.");
        break;
    }
  }

}  // namespace eray::driver::gl

}  // namespace eray::driver::gl

// NOLINTBEGIN
#ifndef IS_DEBUG
#define ERAY_GL_CALL(call) call
#else
#define ERAY_GL_CALL(call)               \
  do {                                   \
    call;                                \
    eray::driver::gl::check_gl_errors(); \
  } while (0)
#endif

#ifdef NDEBUG
#define ERAY_GL_CALL_RET(expr) (expr)
#else
#define ERAY_GL_CALL_RET(expr)           \
  ([&]() {                               \
    auto result = (expr);                \
    eray::driver::gl::check_gl_errors(); \
    return result;                       \
  })()
#endif

// NOLINTEND
