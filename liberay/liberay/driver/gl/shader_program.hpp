#pragma once

#include <glad/gl.h>

#include <liberay/driver/glsl_shader.hpp>
#include <liberay/math/mat.hpp>
#include <liberay/util/string_hash.hpp>
#include <liberay/util/zstring_view.hpp>
#include <string>
#include <unordered_map>

namespace eray::driver::gl {

class ShaderProgram {
 public:
  explicit ShaderProgram(zstring_view name);
  virtual ~ShaderProgram();

  enum class ProgramCreationError : uint8_t {
    LinkingFailed       = 0,
    CompilationFailed   = 1,
    CreationNotPossible = 2,
    ShaderTypeMismatch  = 3,
  };

  void bind() const;
  void unbind() const;

  std::expected<void, ProgramCreationError> recompile();

  template <typename T>
  void set_uniform(zstring_view name, const T& value) const {
    GLint location = get_uniform_location(name);
    if constexpr (std::is_same_v<T, bool>) {
      glProgramUniform1i(program_id_, location, value ? 1 : 0);
    } else if constexpr (std::is_same_v<T, int>) {
      glProgramUniform1i(program_id_, location, value);
    } else if constexpr (std::is_same_v<T, uint32_t>) {
      glProgramUniform1ui(program_id_, location, value);
    } else if constexpr (std::is_same_v<T, float>) {
      glProgramUniform1f(program_id_, location, value);
    } else if constexpr (std::is_same_v<T, math::Vec2f>) {
      glProgramUniform2f(program_id_, location, value.x, value.y);
    } else if constexpr (std::is_same_v<T, math::Vec3f>) {
      glProgramUniform3f(program_id_, location, value.x, value.y, value.z);
    } else if constexpr (std::is_same_v<T, math::Vec4f>) {
      glProgramUniform4f(program_id_, location, value.x, value.y, value.z, value.w);
    } else if constexpr (std::is_same_v<T, math::Mat3f>) {
      glProgramUniformMatrix3fv(program_id_, location, 1, GL_FALSE, value.raw_ptr());
    } else if constexpr (std::is_same_v<T, math::Mat4f>) {
      glProgramUniformMatrix4fv(program_id_, location, 1, GL_FALSE, value.raw_ptr());
    } else {
      static_assert(false, "Unsupported uniform type");
    }
  }

 protected:
  static std::optional<std::string> get_shader_status(GLuint shader, GLenum type);
  static std::optional<std::string> get_program_status(GLuint program, GLenum type);

  virtual std::expected<void, ProgramCreationError> create_program() = 0;
  std::expected<GLuint, ProgramCreationError> create_shader(const GLSLShader& resource, GLenum type);
  std::expected<void, ProgramCreationError> link_program();

  static constexpr zstring_view get_shader_type_name(GLenum shaderType) {
    switch (shaderType) {
      case GL_VERTEX_SHADER:
        return "Vertex Shader";
      case GL_FRAGMENT_SHADER:
        return "Fragment Shader";
      case GL_COMPUTE_SHADER:
        return "Compute Shader";
      default:
        return "Unkown Shader";
    }
  }

  // TODO(migoox): implement shader uniform buffer binding
  //   void bind_uniform_buffer(zstring_view name, const UniformBuffer& ubo) const;
  //   void bind_uniform_buffer(zstring_view name, size_t binding) const;

 private:
  GLint get_uniform_location(zstring_view name) const;

 protected:
  std::string shader_name_;
  GLuint program_id_;

 private:
  mutable std::unordered_map<std::string, GLint, util::StringHash, std::equal_to<>> uniform_locations_;
  mutable std::unordered_map<GLuint, GLuint> uniform_block_bindings_;
};

class RenderingShaderProgram : public ShaderProgram {
 public:
  static std::expected<std::unique_ptr<RenderingShaderProgram>, ProgramCreationError> create(
      zstring_view name, GLSLShader vertex_resource, GLSLShader fragment_resource);

  const GLSLShader& vertex_shader() const { return vertex_shader_; }
  GLSLShader& vertex_shader() { return vertex_shader_; }
  const GLSLShader& fragment_shader() const { return fragment_shader_; }
  GLSLShader& fragment_shader() { return fragment_shader_; }

 private:
  RenderingShaderProgram(zstring_view name, GLSLShader&& vertex_resource, GLSLShader&& fragment_resource);

  std::expected<void, ProgramCreationError> create_program() override;

 private:
  GLSLShader vertex_shader_;
  GLSLShader fragment_shader_;
};

// TODO(migoox): add compute shaders
// class ComputeShaderProgram : public ShaderProgram {
//  public:
//   ComputeShaderProgram(zstring_view name, GLSLShader compute_shader);

//   const GLSLShader& compute_shader() const { return compute_shader_; }
//   GLSLShader& compute_shader() { return compute_shader_; }

//  private:
//   std::expected<void, ProgramCreationError> create_program() override;

//  private:
//   GLSLShader compute_shader_;
// };

}  // namespace eray::driver::gl
