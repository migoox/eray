#pragma once

#include <cstdint>
#include <filesystem>
#include <liberay/math/transform3.hpp>
#include <liberay/res/error.hpp>
#include <liberay/res/image.hpp>
#include <liberay/util/result.hpp>
#include <liberay/util/ruleof.hpp>
#include <unordered_map>

namespace eray::res {

enum class MeshVertexAttributeBits : std::uint8_t {
  Position  = 1U,
  Normal    = 1U << 1,
  Tangent   = 1U << 2,
  Bitangent = 1U << 3,
  TexCoords = 1U << 4,
};

inline MeshVertexAttributeBits operator|(MeshVertexAttributeBits lhs, MeshVertexAttributeBits rhs) {
  return static_cast<MeshVertexAttributeBits>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}
inline MeshVertexAttributeBits operator&(MeshVertexAttributeBits lhs, MeshVertexAttributeBits rhs) {
  return static_cast<MeshVertexAttributeBits>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
}

template <typename Bits>
struct Flags {
  Flags(Bits bits) : mask(static_cast<Bits>(bits)) {}  // NOLINT
  Bits mask;

  bool is_set(Bits bits) { return (mask & bits) != 0U; }
};

using MeshVertexAttributes = Flags<MeshVertexAttributeBits>;

enum class MeshPrimitiveType : std::uint8_t {
  Triangles,
  TrianglesAdjacency,
};

using MaterialId = uint32_t;

/**
 * @brief Stores material info
 *
 */
struct Material {};

struct Node {
  math::Transform3f transform;
};

/**
 * @brief Contains mesh vertex and index buffers. Stores material id that belongs to `Model`. This class cannot be
 * created independently. The `Model` class is responsible for mesh creation.
 *
 */
struct Mesh {
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
  MaterialId mat_id;
};

/**
 * @brief Collection of meshes and materials.
 *
 */
class Model {
 public:
  ERAY_DEFAULT_MOVE_AND_COPY_CTOR(Model)
  ERAY_DEFAULT_MOVE_AND_COPY_ASSIGN(Model)

  /**
   * @brief Currently supports `.gltf` and `.glb`.
   *
   * @param path
   * @return util::Result<VertexArray, FileError>
   */
  [[nodiscard]] static util::Result<Model, FileError> load_from_path(
      const std::filesystem::path& path,
      MeshVertexAttributes vertex_format = MeshVertexAttributeBits::Position | MeshVertexAttributeBits::Normal |
                                           MeshVertexAttributeBits::TexCoords,
      MeshPrimitiveType primitive_type = MeshPrimitiveType::Triangles);

  MeshPrimitiveType primitive_type() const { return primitive_type_; }
  MeshVertexAttributes vertex_format() const { return vertex_format_; }

 private:
  Model();

  MeshPrimitiveType primitive_type_;
  MeshVertexAttributes vertex_format_;

  std::unordered_map<std::string, Mesh> meshes_;
  std::unordered_map<std::string, Node> parent_nodes_;
  std::unordered_map<std::string, Image> images_;
  std::unordered_map<std::string, Material> materials_;
};

}  // namespace eray::res
