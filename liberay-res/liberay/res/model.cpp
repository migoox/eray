#include <expected>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <liberay/res/error.hpp>
#include <liberay/res/file.hpp>
#include <liberay/res/model.hpp>
#include <liberay/util/logger.hpp>

namespace eray::res {

util::Result<Model, FileError> Model::load_from_path(const std::filesystem::path& path,
                                                     MeshVertexAttributes vertex_format,
                                                     MeshPrimitiveType primitive_type) {
  auto extensions        = std::array{".gltf", ".glb"};
  auto validation_result = validate_file(path, extensions);
  if (!validation_result) {
    return std::unexpected(validation_result.error());
  }

  auto gltf_file_opt = fastgltf::GltfDataBuffer::FromPath(path);
  if (!gltf_file_opt) {
    util::Logger::err(R"(Could not load GLTF file from path "{}")", path.string());
    return std::unexpected(FileError{
        .path = path,
        .msg  = "fastgltf failed to load the file",
        .code = FileErrorCode::ReadFailure,
    });
  }
  auto data = std::move(gltf_file_opt.get());
  auto type = fastgltf::determineGltfFileType(data);

  auto asset  = fastgltf::Asset{};
  auto parser = fastgltf::Parser{};
  if (type == fastgltf::GltfType::glTF) {
    auto result_opt = parser.loadGltf(data, path.parent_path());
    if (!result_opt) {
      util::Logger::err(R"(File with path "{}" has incorrect format, expected valid GLTF or GLB file, parsing failed)",
                        path.string());
      return std::unexpected(FileError{
          .path = path,
          .msg  = "fastgltf failed to parse the file",
          .code = FileErrorCode::IncorrectFormat,
      });
    }
    asset = std::move(result_opt.get());

  } else if (type == fastgltf::GltfType::GLB) {
    auto result_opt = parser.loadGltfBinary(data, path.parent_path());
    if (!result_opt) {
      util::Logger::err(R"(File with path "{}" has incorrect format, expected valid GLTF or GLB file, parsing failed)",
                        path.string());
      return std::unexpected(FileError{
          .path = path,
          .msg  = "fastgltf failed to parse the file",
          .code = FileErrorCode::IncorrectFormat,
      });
    }
    asset = std::move(result_opt.get());

  } else {  // fastgltf::GltfType::Invalid
    util::Logger::err(R"(File with path "{}" has incorrect format, expected valid GLTF or GLB file)", path.string());
    return std::unexpected(FileError{
        .path = path,
        .msg  = "fastgltf failed to load the file",
        .code = FileErrorCode::IncorrectFormat,
    });
  }
}

}  // namespace eray::res
