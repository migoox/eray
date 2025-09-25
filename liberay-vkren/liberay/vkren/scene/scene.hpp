#pragma once

#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/scene/entity_pool.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <liberay/vkren/scene/sparse_set.hpp>
#include <liberay/vkren/scene/transform_tree.hpp>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

template <typename... TValues>
using EntitySparseSet = SparseSet<EntityIndex, TValues...>;

using Name               = std::string;
using PrimitiveMeshArray = std::vector<MeshPrimitiveId>;

struct GPUMeshPrimitive {
  vk::Buffer vertex_buffer;
  vk::Buffer index_buffer;
  uint32_t first_index;
  uint32_t index_count;
};

struct GPUMaterial {
  vk::Pipeline pipeline;
  vk::PipelineLayout layout;
  vk::DescriptorSet material_set;
};

struct Material {
  // TODO(migoox): add PBR CPU material members
};

struct Light {
  math::Vec3f direction;
  math::Vec3f position;
  math::Vec3f color;
  float theta{};
  float range{};

  enum class Type : std::uint8_t {
    Directional,
    Spotlight,
    Point,
  };
};

struct Camera {
  float aspect_ratio;
  float fov;
  bool orthographic;
};

struct Scene {
 public:
  const TransformTree& tree() const { return tree_; }
  TransformTree& tree() { return tree_; }

 private:
  TransformTree tree_;
  SparseSet<EntityIndex, MeshId> mesh_nodes_;
  SparseSet<EntityIndex, Camera> camera_nodes_;
  SparseSet<EntityIndex, Light> light_nodes_;

  struct Meshes {
    EntityPool<MeshId> pool;

    /**
     * @brief Every mesh has it's name and PrimitiveMeshArray that might be empty.
     *
     */
    EntitySparseSet<Name, PrimitiveMeshArray> data;

    // in the future: array of weights to be applied to the morph targets
  } meshes_;

  struct PrimitiveMeshes {
    EntityPool<MeshPrimitiveId> pool;

    /**
     * @brief Every primitive mesh has it's GPUMeshPrimitive, but not necessarily MaterialId which might be null.
     *
     */
    EntitySparseSet<MaterialId, GPUMeshPrimitive> data;

  } primitive_meshes_;

  struct Materials {
    EntityPool<MaterialId> pool;

    /**
     * @brief Every material has it's GPUMaterial.
     *
     */
    EntitySparseSet<Material, GPUMaterial> data;  // every material contains it's gpu data
  } materials_;
};

}  // namespace eray::vkren
