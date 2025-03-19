
#include <algorithm>
#include <cstddef>
#include <expected>
#include <liberay/driver/gl/shader_program.hpp>
#include <liberay/driver/glsl_shader.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/try.hpp>
#include <memory>
#include <optional>

#include "liberay/driver/gl/gl_error.hpp"

namespace eray::driver::gl {

ShaderProgram::ShaderProgram(zstring_view name) : shader_name_(name), program_id_(glCreateProgram()) {
  if (program_id_ == 0) {
    throw std::runtime_error(std::format("Unable to create shader {}", name));
  }
}

ShaderProgram::~ShaderProgram() { glDeleteProgram(program_id_); }

void ShaderProgram::bind() const { glUseProgram(program_id_); }

void ShaderProgram::unbind() const { glUseProgram(0); }  // NOLINT

std::expected<void, ShaderProgram::ProgramCreationError> ShaderProgram::recompile() {
  using clock = std::chrono::high_resolution_clock;
  auto start  = clock::now();

  glDeleteProgram(program_id_);
  program_id_ = glCreateProgram();

  TRY(create_program());

  auto stop     = clock::now();
  auto duration = duration_cast<std::chrono::milliseconds>(stop - start);
  util::Logger::debug("Shader {} recompilation took {}", shader_name_, duration);

  // Reconnect UBO bindings
  for (auto& pair : uniform_block_bindings_) {
    glUniformBlockBinding(program_id_, pair.first, pair.second);
  }

  return {};
}

std::optional<std::string> ShaderProgram::get_shader_status(GLuint shader, GLenum type) {
  GLint status = 0;
  glGetShaderiv(shader, type, &status);
  if (status == GL_FALSE) {
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    std::string info(static_cast<GLuint>(length), '\0');
    glGetShaderInfoLog(shader, length, &length, info.data());
    return info;
  }

  return std::nullopt;
}

std::optional<std::string> ShaderProgram::get_program_status(GLuint program, GLenum type) {
  GLint status = 0;
  glGetProgramiv(program, type, &status);
  if (status == GL_FALSE) {
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

    std::string info(static_cast<GLuint>(length), '\0');
    glGetProgramInfoLog(program, length, &length, info.data());
    return info;
  }

  return std::nullopt;
}

std::expected<void, ShaderProgram::ProgramCreationError> ShaderProgram::link_program() {
  glLinkProgram(program_id_);

  auto link_status = get_program_status(program_id_, GL_LINK_STATUS);
  if (link_status.has_value()) {
    util::Logger::err("Shader program linking failed for shader {}.", shader_name_);
    return std::unexpected(ProgramCreationError::LinkingFailed);
  }

  glValidateProgram(program_id_);

  auto validate_status = get_program_status(program_id_, GL_VALIDATE_STATUS);
  if (validate_status.has_value()) {
    util::Logger::err("Shader linking ({}) validation failed with status: {}.", shader_name_, validate_status.value());
    return std::unexpected(ProgramCreationError::LinkingFailed);
  }

  return {};
}

GLint ShaderProgram::get_uniform_location(zstring_view name) const {
  auto it = uniform_locations_.find(name);
  if (it != uniform_locations_.end()) {
    return it->second;
  }

  const std::string key(name);
  const GLint location    = glGetUniformLocation(program_id_, key.c_str());
  uniform_locations_[key] = location;
  if (location == -1) {
    util::Logger::err(R"(Unable to find uniform "{}" in shader "{}")", name, shader_name_);
    return location;
  }

  util::Logger::debug(R"(Caching new uniform location: "{}" = {})", name, location);
  return location;
}

// TODO(migoox): return GLName wrapper instead of the raw id
std::expected<GLuint, ShaderProgram::ProgramCreationError> ShaderProgram::create_shader(const GLSLShader& resource,
                                                                                        GLenum type) {
  const GLuint shader = glCreateShader(type);

  if (shader == 0) {
    util::Logger::err(R"(Unable to create a shader program)");
    return std::unexpected(ProgramCreationError::CreationNotPossible);
  }

  const GLchar* source = resource.get_glsl().c_str();
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  auto compile_status = get_shader_status(shader, GL_COMPILE_STATUS);
  if (compile_status.has_value()) {
    util::Logger::err(R"(Shader program compilation failed for shader {}, with status: {})", shader_name_,
                      compile_status.value());
    return std::unexpected(ProgramCreationError::CompilationFailed);
  }

  return shader;
}

// void ShaderProgram::bind_uniform_buffer(zstring_view name, const UniformBuffer& ubo) const {
//   const GLuint index = glGetUniformBlockIndex(program_id_, name.data());
//   if (index == GL_INVALID_INDEX) {
//     log_throw(ShaderProgramValidationException(
//         shader_name_, std::format(R"(Unable to find uniform block "{}" in shader "{}")", name, shader_name_)));
//   }

//   GLint size = 0;
//   glGetActiveUniformBlockiv(program_id_, index, GL_UNIFORM_BLOCK_DATA_SIZE, &size);

//   // According to std140 standard the padding at the end of an array MAY be added
//   // https://registry.khronos.org/OpenGL/specs/gl/glspec45.core.pdf#page=159
//   if (size != static_cast<GLint>(ubo.buffer_size()) &&
//       size != static_cast<GLint>(ubo.buffer_size_without_end_padding())) {
//     log_throw(ShaderProgramValidationException(
//         shader_name_, std::format(R"(Unexpected uniform block size: {}. Expected: {} or {})", size,
//         ubo.buffer_size(),
//                                   ubo.buffer_size_without_end_padding())));
//   }

//   uniform_block_bindings_[index] = static_cast<GLuint>(ubo.binding());
//   glUniformBlockBinding(program_id_, index, uniform_block_bindings_[index]);
//   util::Logger::debug(R"(Bound uniform block "{}" (index: {}, size: {} bytes) in shader "{}" to binding: {})", name,
//                       index, size, shader_name_, ubo.binding());
// }

// void ShaderProgram::bind_uniform_buffer(zstring_view name, size_t binding) const {
//   const GLuint index = glGetUniformBlockIndex(program_id_, name.data());
//   if (index == GL_INVALID_INDEX) {
//     log_throw(ShaderProgramValidationException(
//         shader_name_, std::format(R"(Unable to find uniform block "{}" in shader "{}")", name, shader_name_)));
//   }

//   uniform_block_bindings_[index] = static_cast<GLuint>(binding);
//   glUniformBlockBinding(program_id_, index, uniform_block_bindings_[index]);
//   util::Logger::debug(R"(Bound uniform block "{}" (index: {}) in shader "{}" to binding: {})", name, index,
//                       shader_name_, binding);
// }

RenderingShaderProgram::RenderingShaderProgram(zstring_view name, GLSLShader&& vert_shader, GLSLShader&& frag_shader,
                                               std::optional<GLSLShader>&& tesc_shader,
                                               std::optional<GLSLShader>&& tese_shader)
    : ShaderProgram(name),
      vertex_shader_(std::move(vert_shader)),
      fragment_shader_(std::move(frag_shader)),
      tesc_shader_(std::move(tesc_shader)),
      tese_shader_(std::move(tese_shader)) {}

std::expected<std::unique_ptr<RenderingShaderProgram>, RenderingShaderProgram::ProgramCreationError>
RenderingShaderProgram::create(zstring_view name, GLSLShader vert_shader, GLSLShader frag_shader,
                               std::optional<GLSLShader> tesc_shader, std::optional<GLSLShader> tese_shader) {
  if (vert_shader.get_type() != ShaderType::Vertex) {
    util::Logger::err("Shader type mismatched. Expected .vert, but received {}.", vert_shader.get_extension());
    return std::unexpected(ProgramCreationError::ShaderTypeMismatch);
  }

  if (frag_shader.get_type() != ShaderType::Fragment) {
    util::Logger::err("Shader type mismatched. Expected .frag, but received {}.", frag_shader.get_extension());
    return std::unexpected(ProgramCreationError::ShaderTypeMismatch);
  }

  if ((tesc_shader && !tese_shader) || (!tesc_shader && tese_shader)) {
    util::Logger::err("Only one of the tesselation shaders has been provided.");
    return std::unexpected(ProgramCreationError::TesselationShaderWithoutItsPair);
  }

  if (tesc_shader) {
    if (tesc_shader->get_type() != ShaderType::TessControl) {
      util::Logger::err("Shader type mismatched. Expected .tesc, but received {}.", tesc_shader->get_extension());
      return std::unexpected(ProgramCreationError::ShaderTypeMismatch);
    }

    if (tese_shader->get_type() != ShaderType::TessEval) {
      util::Logger::err("Shader type mismatched. Expected .tese, but received {}.", tese_shader->get_extension());
      return std::unexpected(ProgramCreationError::ShaderTypeMismatch);
    }
  }

  // TODO(migoox): Solve the move semantics problem (create a GLName class for automatic move semantics)
  auto program = std::unique_ptr<RenderingShaderProgram>(new RenderingShaderProgram(
      name, std::move(vert_shader), std::move(frag_shader), std::move(tesc_shader), std::move(tese_shader)));

  using clock = std::chrono::high_resolution_clock;
  auto start  = clock::now();

  TRY(program->create_program());

  auto stop     = clock::now();
  auto duration = duration_cast<std::chrono::milliseconds>(stop - start);
  util::Logger::debug("Shader {} creation took {}", name, duration);

  return program;
}

std::expected<void, RenderingShaderProgram::ProgramCreationError> RenderingShaderProgram::create_program() {
  TRY_UNWRAP_DEFINE(vertex_shader, create_shader(vertex_shader_, GL_VERTEX_SHADER));
  TRY_UNWRAP_DEFINE(fragment_shader, create_shader(fragment_shader_, GL_FRAGMENT_SHADER));
  glAttachShader(program_id_, vertex_shader);
  glAttachShader(program_id_, fragment_shader);

  std::optional<GLuint> tesc_shader = std::nullopt;
  std::optional<GLuint> tese_shader = std::nullopt;
  if (tesc_shader_) {
    TRY_UNWRAP_ASSIGN(tesc_shader, create_shader(*tesc_shader_, GL_TESS_CONTROL_SHADER));
    TRY_UNWRAP_ASSIGN(tese_shader, create_shader(*tese_shader_, GL_TESS_EVALUATION_SHADER));
    glAttachShader(program_id_, *tesc_shader);
    glAttachShader(program_id_, *tese_shader);
  }

  TRY(link_program());

  // TODO(migoox): Fix this when UNWRAP fails
  glDetachShader(program_id_, vertex_shader);
  glDetachShader(program_id_, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  if (tesc_shader_) {
    glDetachShader(program_id_, *tesc_shader);
    glDetachShader(program_id_, *tese_shader);
    glDeleteShader(*tesc_shader);
    glDeleteShader(*tese_shader);
  }

  return {};
}

// ComputeShaderProgram::ComputeShaderProgram(zstring_view name, GLSLShader compute_shader)
//     : ShaderProgram(name), compute_shader_(std::move(compute_shader)) {
//   if (compute_shader_.get_type() != ShaderType::Compute) {
//     log_throw(ShaderTypeMismatchException(get_shader_type_name(GL_COMPUTE_SHADER), shader_name_,
//                                           compute_shader_.get_extension()));
//   }

//   using clock = std::chrono::high_resolution_clock;
//   auto start  = clock::now();

//   TRY(create_program());

//   auto stop     = clock::now();
//   auto duration = duration_cast<std::chrono::milliseconds>(stop - start);
//   util::Logger::info("Shader {} creation took {}", name, duration);
// }

// void ComputeShaderProgram::create_program() {
//   GLuint compute_shader = create_shader(compute_shader_, GL_COMPUTE_SHADER);
//   glAttachShader(program_id_, compute_shader);

//   link_program();

//   glDetachShader(program_id_, compute_shader);
//   glDeleteShader(compute_shader);
// }

}  // namespace eray::driver::gl
