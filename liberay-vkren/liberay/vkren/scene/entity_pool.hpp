#pragma once
#include <liberay/vkren/scene/basic_object_pool.hpp>
#include <limits>

namespace eray::vkren {

using EntityIndex = size_t;

/**
 * @brief Node, mesh, material are entities. Do not confuse with ECS.
 *
 */
template <typename TTag>
struct EntityId {
  using Tag = TTag;

  /**
   * @brief LSB 32 bits store node index and the rest of the 32 bits store the node version.
   *
   */
  uint64_t value{};

  constexpr explicit EntityId(uint64_t v = 0) : value(v) {}
  constexpr auto operator<=>(const EntityId&) const = default;
};

template <typename TTag>
struct EntityIdExtractor {
  static constexpr size_t kNullIndex      = std::numeric_limits<size_t>::max();
  static constexpr EntityId<TTag> kNullId = EntityId<TTag>{std::numeric_limits<uint64_t>::max()};

  [[nodiscard]] static size_t index_of(EntityId<TTag> id) { return static_cast<size_t>(id.value & 0xFFFFFFFF); }
  [[nodiscard]] static uint32_t version_of(EntityId<TTag> id) {
    auto version = static_cast<uint32_t>(id.value >> 32);
    return version;
  }
  [[nodiscard]] static EntityId<TTag> compose_id(size_t index, uint32_t version) {
    return EntityId<TTag>{(static_cast<uint64_t>(version) << 32) | static_cast<uint64_t>(index)};
  }
};

template <typename TEntityId>
using EntityPool = BasicObjectPool<TEntityId, EntityIdExtractor<typename TEntityId::Tag>>;

// == Entities =========================================================================================================

struct NodeTag {};
using NodeId = EntityId<NodeTag>;

struct MeshTag {};
using MeshId = EntityId<MeshTag>;

struct MeshPrimitiveTag {};
using MeshSurfaceId = EntityId<MeshPrimitiveTag>;

struct MaterialTag {};
using MaterialId = EntityId<MaterialTag>;

struct ShaderTag {};
using ShaderId = EntityId<ShaderTag>;

struct LightTag {};
using LightId = EntityId<LightTag>;

struct CameraTag {};
using CameraId = EntityId<CameraTag>;

}  // namespace eray::vkren

// == Id Hash ==========================================================================================================

namespace std {
template <typename Tag>
struct hash<eray::vkren::EntityId<Tag>> {
  size_t operator()(eray::vkren::EntityId<Tag> id) const noexcept { return std::hash<uint64_t>()(id.value); }
};
}  // namespace std
