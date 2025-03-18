#include <algorithm>
#include <expected>
#include <filesystem>
#include <fstream>
#include <functional>
#include <liberay/driver/glsl_shader.hpp>
#include <liberay/util/logger.hpp>
#include <liberay/util/string_views.hpp>
#include <unordered_map>
#include <unordered_set>

namespace eray::driver {

GLSLShader::GLSLShader(std::string&& content, ShaderType type, std::unordered_set<std::string>&& ext_defi_names,
                       std::optional<std::string>&& version, std::filesystem::path&& path)
    : ext_defi_names_(std::move(ext_defi_names)),
      path_(std::move(path)),
      version_(std::move(version)),
      raw_content_(std::move(content)),
      type_(type),
      is_dirty_(true) {}

void GLSLShader::set_ext_defi(std::string_view ext_defi_name, std::string&& defi_content) {
  auto it = std::ranges::find(ext_defi_names_, ext_defi_name);
  if (it == ext_defi_names_.end()) {
    util::Logger::warn("Shader loaded from path {} does not contain external definitions named \"{}\"", path_.string(),
                       ext_defi_name);
  }

  ext_defi_contents_[*it] = std::move(defi_content);
  is_dirty_               = true;
}

bool GLSLShader::is_glsl_ready() const { return ext_defi_contents_.size() == ext_defi_names_.size(); }

const std::string& GLSLShader::get_glsl() const {
  if (!is_dirty_) {
    return glsl_;
  }

  // TODO(migoox): shaders with external definitions and without them should probably be splitted to
  // 2 separate classes (single responsibility rule) -- DynamicShader and StaticShader
  glsl_.clear();
  if (version_) {
    glsl_.append(*version_);
  }
  glsl_.append("\n");
  for (const auto& ext_defi : ext_defi_contents_) {
    glsl_.append(std::format("#define {} {}\n", ext_defi.first, ext_defi.second));
  }
  glsl_.append(raw_content_);

  is_dirty_ = false;
  return glsl_;
}
std::expected<ShaderType, GLSLShaderManager::LoadingError> GLSLShaderManager::get_sh_type(
    const std::filesystem::path& path) {
  auto file_ext = path.extension().string();
  auto sh_type  = kShaderTypeToExtensions.from_value(file_ext);
  if (!sh_type) {
    util::Logger::err(R"(File extension " {} " is not supported for "{}")", path.string(), file_ext);
    return std::unexpected(LoadingError::FileExtensionNotSupported);
  }

  return *sh_type;
}

std::expected<std::string, GLSLShaderManager::LoadingError> GLSLShaderManager::load_content(
    const std::filesystem::path& path) {
  namespace fs = std::filesystem;

  if (!fs::exists(path)) {
    util::Logger::err(R"(File "{}" does note exist.)", path.string());
    return std::unexpected(LoadingError::FileDoesNotExist);
  }

  if (!fs::is_regular_file(path) && !fs::is_symlink(path)) {
    util::Logger::err("Expected regular or symlink for file {}.", path.string());
    return std::unexpected(LoadingError::InvalidFileType);
  }

  std::ifstream file_stream(path);
  if (!file_stream.is_open()) {
    util::Logger::err("File stream not available for file {}.", path.string());
    return std::unexpected(LoadingError::FileStreamNotAvailable);
  }

  std::stringstream buffer;
  buffer << file_stream.rdbuf();
  return buffer.str();
}

std::expected<void, GLSLShaderManager::LoadingError> GLSLShaderManager::process_include_macro(
    const std::filesystem::path& sh_path, util::WordsStringViewIterator& it, const util::WordsStringViewIterator& end,
    size_t curr_line, std::string& content, std::unordered_set<std::string>& defi_names) {
  if (it == end) {
    util::Logger::err(R"(Shader ("{}") parsing error: Invalid macro ({}) argument count. (line: {}))", sh_path.string(),
                      std::string(internal::kExtDefiMacro), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }
  auto arg = std::string_view{*it};

  ++it;
  if (it != end) {
    util::Logger::err(R"(Shader ("{}") parsing error: Invalid macro ({}) argument count. (line: {}))", sh_path.string(),
                      std::string(internal::kVersionMacro), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }

  if (!arg.starts_with("\"") || !arg.ends_with("\"") || arg.size() < 2) {
    util::Logger::err(R"(Shader ("{}") parsing error: Include macro should begin and end with `\"`. (line: {}))",
                      sh_path.string(), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }

  auto arg_val  = std::string_view{arg.substr(1, arg.size() - 2)};
  auto rel_path = std::filesystem::path{arg_val};
  if (rel_path.empty()) {
    util::Logger::err(R"(Shader ("{}") parsing error: The include macro argument cannot be empty. (line: {}))",
                      sh_path.string(), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }

  if (rel_path.is_absolute()) {
    util::Logger::err(
        R"(Shader ("{}") parsing error: The include macro argument cannot be an absolute path. (line: {}))",
        sh_path.string(), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }

  auto dep_ext = kShaderTypeToExtensions.from_value(rel_path.extension().string());
  if (!dep_ext.has_value() || dep_ext.value() != ShaderType::Library) {
    util::Logger::err(
        R"(Shader ("{}") parsing error: The include macro argument must be a library shader (must have ".glsl" extension). (line: {}))",
        sh_path.string(), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }

  auto abs_path = sh_path.parent_path() / rel_path;
  if (abs_path == sh_path || std::ranges::find(visited_paths_, abs_path) != visited_paths_.end()) {
    util::Logger::err(R"(Shader ("{}") parsing error: Detected a dependency cycle. (line: {}))", sh_path.string(),
                      curr_line);
    return std::unexpected(LoadingError::IncludeDependencyCycle);
  }

  visited_paths_.push_back(abs_path);
  auto res = load_library_shader(abs_path);
  visited_paths_.pop_back();

  if (!res) {
    return std::unexpected(res.error());
  }

  content.append(res->get().get_raw());
  defi_names.insert(res->get().get_ext_defi_names().begin(), res->get().get_ext_defi_names().end());

  return {};
}

std::expected<void, GLSLShaderManager::LoadingError> GLSLShaderManager::process_ext_defi_macro(
    const std::filesystem::path& sh_path, util::WordsStringViewIterator& it, const util::WordsStringViewIterator& end,
    size_t curr_line, std::unordered_set<std::string>& defi_names) {
  if (it == end) {
    util::Logger::err(R"(Shader ("{}") parsing error: Invalid macro ({}) argument count. (line: {}))", sh_path.string(),
                      std::string(internal::kExtDefiMacro), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }
  auto arg = std::string_view{*it};

  ++it;
  if (it != end) {
    util::Logger::err(R"(Shader ("{}") parsing error: Invalid macro ({}) argument count. (line: {}))", sh_path.string(),
                      std::string(internal::kExtDefiMacro), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }

  if (!std::ranges::all_of(arg, [](const char c) { return std::isalnum(c) != 0 || c == '_'; })) {
    util::Logger::err(
        R"(Shader ("{}") parsing error: The external definition macro argument must not contain non-alphanumeric characters. (line: {}))",
        sh_path.string(), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }

  defi_names.emplace(arg);

  return {};
}

std::expected<std::optional<std::string>, GLSLShaderManager::LoadingError> GLSLShaderManager::process_version_macro(
    const std::filesystem::path& sh_path, ShaderType sh_type, util::WordsStringViewIterator& it,
    const util::WordsStringViewIterator& end, size_t curr_line) {
  if (sh_type == ShaderType::Library) {
    util::Logger::warn("Ignoring version macro in .glsl shader.");
    return std::nullopt;
  }

  if (it == end) {
    util::Logger::err(R"(Shader ("{}") parsing error: Invalid macro ({}) argument count. (line: {}))", sh_path.string(),
                      std::string(internal::kVersionMacro), curr_line);
    return std::unexpected(LoadingError::ParsingError);
  }
  auto arg1 = std::string_view{*it};

  ++it;
  if (it != end) {
    auto arg2 = std::string_view{*it};

    ++it;
    if (it != end) {
      util::Logger::err(R"(Shader ("{}") parsing error: Invalid macro ({}) argument count. (line: {}))",
                        sh_path.string(), std::string(internal::kVersionMacro), curr_line);
      return std::unexpected(LoadingError::ParsingError);
    }

    return std::format("#version {} {}", arg1, arg2);
  }

  return std::format("#version {}", arg1);
}

std::expected<GLSLShader, GLSLShaderManager::LoadingError> GLSLShaderManager::load_shader(
    const std::filesystem::path& path) {
  util::Logger::info("Loading a shader with path \"{}\"...", path.string());

  auto sh_type = get_sh_type(path);
  if (!sh_type) {
    return std::unexpected(sh_type.error());
  }

  auto content = load_content(path);
  if (!content) {
    return std::unexpected(content.error());
  }

  auto lines = util::make_lines_view(*content) | std::views::enumerate;

  std::unordered_set<std::string> defi_names;
  std::string preprocessed_content;
  std::optional<std::string> version;

  for (auto const [l, line_str] : lines) {
    auto line  = static_cast<size_t>(l);
    auto words = util::make_words_view(line_str);

    auto it  = words.begin();
    auto end = words.end();

    if (it == end) {
      if (line_str != "") {
        preprocessed_content.append(line_str);
        preprocessed_content.append("\n");
      }
      continue;
    }

    auto macro = std::string_view{*it};
    if (!internal::is_macro(macro)) {
      preprocessed_content.append(line_str);
      preprocessed_content.append("\n");
      continue;
    }

    ++it;
    if (macro == internal::kIncludeMacro) {
      auto result = process_include_macro(path, it, end, line, preprocessed_content, defi_names);
      if (!result) {
        return std::unexpected(result.error());
      }

    } else if (macro == internal::kVersionMacro) {
      if (version != std::nullopt) {
        continue;
      }
      auto result = process_version_macro(path, *sh_type, it, end, line);
      if (!result) {
        return std::unexpected(result.error());
      }

      version = std::move(*result);
    } else {
      auto result = process_ext_defi_macro(path, it, end, line, defi_names);
      if (!result) {
        return std::unexpected(result.error());
      }
    }
  }

  if (*sh_type != ShaderType::Library && !version.has_value()) {
    util::Logger::err(R"(Shader ("{}") parsing error: No version macro detected.)", path.string(),
                      std::string(internal::kVersionMacro));
    return std::unexpected(LoadingError::NoVersionProvided);
  }

  util::Logger::succ("Loaded a shader with path \"{}\".", path.string());

  return GLSLShader(std::move(preprocessed_content), *sh_type, std::move(defi_names), std::move(version),
                    std::filesystem::path(path));
}

std::expected<std::reference_wrapper<GLSLShader>, GLSLShaderManager::LoadingError>
GLSLShaderManager::load_library_shader(const std::filesystem::path& path) {
  auto cache_it = cache_.find(path);
  if (cache_it != cache_.end()) {
    util::Logger::debug("Loaded a library shader with path \"{}\" from cache.", path.string());
    return cache_it->second;
  }

  auto sh = load_shader(path);
  if (!sh) {
    return std::unexpected(sh.error());
  }
  auto result = cache_.emplace(path, std::move(*sh));

  return result.first->second;
}

}  // namespace eray::driver
