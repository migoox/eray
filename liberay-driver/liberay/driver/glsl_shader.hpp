#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <liberay/util/enum_mapper.hpp>
#include <liberay/util/ruleof.hpp>
#include <liberay/util/string_views.hpp>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace eray::driver {

namespace internal {

static constexpr std::string_view kIncludeMacro = "#include";
static constexpr std::string_view kExtDefiMacro = "#external_definition";
static constexpr std::string_view kVersionMacro = "#version";

static constexpr std::array<std::string_view, 3> kAllMacros = {
    kIncludeMacro,
    kExtDefiMacro,
    kVersionMacro,
};

inline bool is_macro(std::string_view word) { return std::ranges::find(kAllMacros, word) != kAllMacros.end(); }

}  // namespace internal

enum class ShaderType : uint8_t {
  Vertex      = 0,
  Fragment    = 1,
  TessControl = 2,
  TessEval    = 3,
  Geometric   = 4,
  Compute     = 5,
  Library     = 6,
  _Count      = 7  // NOLINT
};

static constexpr auto kShaderTypeToExtensions = util::StringEnumMapper<ShaderType>({
    {ShaderType::Vertex, ".vert"},
    {ShaderType::Fragment, ".frag"},
    {ShaderType::TessControl, ".tesc"},
    {ShaderType::TessEval, ".tese"},
    {ShaderType::Geometric, ".geom"},
    {ShaderType::Compute, ".comp"},
    {ShaderType::Library, ".glsl"},
});

class GLSLShaderManager;

class GLSLShader {
 public:
  GLSLShader() = delete;

  const std::unordered_set<std::string>& get_ext_defi_names() const { return ext_defi_names_; }

  /**
   * @brief Allows to set a value for external definition.
   *
   * @param ext_defi_name
   * @param defi_content
   * @return void
   */
  void set_ext_defi(std::string_view ext_defi_name, std::string&& defi_content);

  /**
   * @brief Checks if the shader is ready. If shader has no external definitions it's always ready. If it
   * contains external defintions, this function checks whether all of them are set.
   *
   * @return true
   * @return false
   */
  bool is_glsl_ready() const;

  /**
   * @brief Returns glsl shader string with inserted dependencies, external defintions (if there are any) and version
   * macro.
   *
   * @return const std::string&
   */
  const std::string& get_glsl() const;

  /**
   * @brief Returns shader type (e.g. Fragment Shader).
   *
   * @return ShaderType
   */
  ShaderType get_type() const { return type_; }

  /**
   * @brief Returns shader extension string literal (e.g. ".frag").
   *
   * @return ShaderType
   */
  util::zstring_view get_extension() const { return kShaderTypeToExtensions[type_]; }

 protected:
  friend GLSLShaderManager;

  GLSLShader(std::string&& content, ShaderType type, std::unordered_set<std::string>&& ext_defi_names,
             std::optional<std::string>&& version, std::filesystem::path&& path);

  const std::string& get_raw() const { return raw_content_; }

 private:
  std::unordered_set<std::string> ext_defi_names_;
  std::unordered_map<std::string, std::string> ext_defi_contents_;
  std::filesystem::path path_;

  std::optional<std::string> version_;
  std::string raw_content_;
  ShaderType type_;

  mutable bool is_dirty_;
  mutable std::string glsl_;
};

class GLSLShaderManager {
 public:
  ERAY_DELETE_COPY_AND_MOVE(GLSLShaderManager)

  GLSLShaderManager()  = default;
  ~GLSLShaderManager() = default;

  enum class LoadingError : uint8_t {
    FileExtensionNotSupported = 0,
    FileDoesNotExist          = 1,
    InvalidFileType           = 2,
    FileStreamNotAvailable    = 3,
    ParsingError              = 4,
    IncludeDependencyCycle    = 5,
    NoVersionProvided         = 6,
  };

  /**
   * @brief Loads glsl shader and caches it's dependencies (library shaders with ".glsl" extension)
   * from absolute path.
   *
   * @return ShaderType
   */
  std::expected<GLSLShader, LoadingError> load_shader(const std::filesystem::path& path);

 private:
  std::expected<std::reference_wrapper<GLSLShader>, GLSLShaderManager::LoadingError> load_library_shader(
      const std::filesystem::path& path);

  static std::expected<ShaderType, LoadingError> get_sh_type(const std::filesystem::path& path);

  static std::expected<std::string, LoadingError> load_content(const std::filesystem::path& path);

  std::expected<void, LoadingError> process_include_macro(const std::filesystem::path& sh_path,
                                                          util::WordsStringViewIterator& it,
                                                          const util::WordsStringViewIterator& end, size_t curr_line,
                                                          std::string& content,
                                                          std::unordered_set<std::string>& defi_names);

  static std::expected<void, LoadingError> process_ext_defi_macro(const std::filesystem::path& sh_path,
                                                                  util::WordsStringViewIterator& it,
                                                                  const util::WordsStringViewIterator& end,
                                                                  size_t curr_line,
                                                                  std::unordered_set<std::string>& defi_names);

  static std::expected<std::optional<std::string>, LoadingError> process_version_macro(
      const std::filesystem::path& sh_path, ShaderType sh_type, util::WordsStringViewIterator& it,
      const util::WordsStringViewIterator& end, size_t curr_line);

 private:
  std::vector<std::filesystem::path> visited_paths_;
  std::unordered_map<std::filesystem::path, GLSLShader> cache_;
};

}  // namespace eray::driver
