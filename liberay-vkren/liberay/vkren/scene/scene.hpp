#pragma once

#include <liberay/math/mat.hpp>
#include <liberay/math/mat_fwd.hpp>
#include <liberay/math/quat.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/scene/flat_tree.hpp>
#include <liberay/vkren/scene/object_pool.hpp>
#include <liberay/vkren/scene/sparse_set.hpp>
#include <liberay/vkren/scene/transform_tree.hpp>
#include <vulkan/vulkan.hpp>

namespace eray::vkren {

using MeshId    = ComposedId3232;
using MeshIndex = uint32_t;

using MeshPrimitiveId    = ComposedId3232;
using MeshPrimitiveIndex = std::uint32_t;

using MaterialId    = ComposedId3232;
using MaterialIndex = std::uint32_t;

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

struct Scene {
 public:
  const TransformTree& tree() const { return tree_; }
  TransformTree& tree() { return tree_; }

 private:
  TransformTree tree_;

  SparseSet<NodeIndex, MeshId> node_meshes_map_;

  struct Meshes {
    ObjectPool3232<MeshId> manager;
    SparseSet<MeshIndex, Name, PrimitiveMeshArray> values;  // every mesh contains name and primitive meshes array

    // in the future: array of weights to be applied to the morph targets
  } meshes_;

  struct PrimitiveMeshes {
    ObjectPool3232<MeshPrimitiveId> manager;
    SparseSet<MeshPrimitiveIndex, GPUMeshPrimitive> gpu_data;  // every mesh contains GPU mesh primitive
    SparseSet<MeshPrimitiveIndex, MaterialId> material;        // but not every mesh contains material

  } primitive_meshes_;

  struct Materials {
    ObjectPool3232<MaterialId> manager;
    SparseSet<MaterialIndex, GPUMaterial> gpu_data;  // every material contains it's gpu data
  } materials_;
};

}  // namespace eray::vkren
