#pragma once

#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/image.hpp>
#include <liberay/vkren/scene/camera.hpp>
#include <liberay/vkren/scene/entity_pool.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <liberay/vkren/scene/light.hpp>
#include <liberay/vkren/scene/material.hpp>
#include <liberay/vkren/scene/sparse_set.hpp>
#include <liberay/vkren/scene/transform_tree.hpp>
#include <unordered_set>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

template <typename... TValues>
using EntitySparseSet = SparseSet<EntityIndex, TValues...>;

struct GPUMeshSurface {
  vk::Buffer vertex_buffer;
  vk::Buffer index_buffer;
  uint32_t first_index;
  uint32_t index_count;
};

struct Scene {
 public:
  const TransformTree& tree() const { return tree_; }
  TransformTree& tree() { return tree_; }

 private:
  TransformTree tree_;
  EntitySparseSet<MeshId> mesh_nodes_;
  EntitySparseSet<Camera> camera_nodes_;
  EntitySparseSet<Light> light_nodes_;

  struct Meshes {
    EntityPool<MeshId> pool;

    /**
     * @brief Every mesh has it's name and MeshSurfacesArray.
     *
     */
    EntitySparseSet<std::string, std::vector<MeshSurfaceId>> data;

    // in the future: array of weights to be applied to the morph targets
  } meshes_;

  struct PrimitiveMeshes {
    EntityPool<MeshSurfaceId> pool;

    /**
     * @brief Every primitive mesh has it's GPUMeshPrimitive, but not necessarily MaterialId which might be null.
     *
     */
    EntitySparseSet<MaterialId, GPUMeshSurface, std::unordered_set<MeshId>> data;

  } primitive_meshes_;

  struct Materials {
    EntityPool<MaterialId> pool;

    /**
     * @brief Every material has it's gpu data.
     *
     */
    EntitySparseSet<Material, GPUMaterial, std::unordered_set<MeshSurfaceId>> data;
  } materials_;

  struct Textures {
    EntityPool<TextureId> textures;

    EntitySparseSet<ImageResource, std::unordered_set<MaterialId>> data;
  };
};

}  // namespace eray::vkren
