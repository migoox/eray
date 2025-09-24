#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eray::vkren {

template <typename T, typename TId>
concept CComposedIdExtractor = requires(std::size_t index, std::uint32_t version, TId id) {
  { T::index_of(id) } -> std::same_as<size_t>;
  { T::version_of(id) } -> std::same_as<uint32_t>;
  { T::compose_id(index, version) } -> std::same_as<TId>;
};

template <typename TComposedId, CComposedIdExtractor<TComposedId> TIdExtractor>
class BasicObjectPool {
 public:
  using Extractor = TIdExtractor;

  TComposedId create() {
    auto ind = free_.back();
    ++obj_count_;
    return TIdExtractor::compose_id(ind, version_[ind]);
  }

  void remove(TComposedId id) {
    --obj_count_;
    version_[TIdExtractor::index_of(id)]++;
  }

  bool exists(TComposedId id) const { return version_[id] == TIdExtractor::version_of(id); }
  size_t obj_count() const { return obj_count_; }

 private:
  std::vector<uint32_t> version_;
  std::vector<size_t> free_;
  size_t obj_count_{};
};

using ComposedId3232      = uint64_t;
using ComposedId3232Index = uint32_t;
struct ComposedId3232Extractor {
  [[nodiscard]] static size_t index_of(ComposedId3232 id) { return static_cast<size_t>(id & 0xFFFFFFFF); }

  [[nodiscard]] static uint32_t version_of(ComposedId3232 id) {
    auto version = static_cast<uint32_t>(id >> 32);
    return version;
  }

  [[nodiscard]] static ComposedId3232 compose_id(size_t index, uint32_t version) {
    return (static_cast<uint64_t>(version) << 32) | static_cast<uint64_t>(index);
  }
};

template <typename TId>
  requires std::same_as<TId, ComposedId3232>
using ObjectPool3232 = BasicObjectPool<TId, ComposedId3232Extractor>;

}  // namespace eray::vkren
