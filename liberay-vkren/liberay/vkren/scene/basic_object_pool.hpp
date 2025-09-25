#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <vector>

namespace eray::vkren {

template <typename T, typename TComposedId>
concept CComposedIdExtractor = requires(std::size_t index, std::uint32_t version, TComposedId id) {
  { T::index_of(id) } -> std::same_as<size_t>;
  { T::version_of(id) } -> std::same_as<uint32_t>;
  { T::compose_id(index, version) } -> std::same_as<TComposedId>;
  { T::kNullId } -> std::convertible_to<TComposedId>;
  { T::kNullIndex } -> std::convertible_to<size_t>;
};

template <typename TComposedId, CComposedIdExtractor<TComposedId> TIdExtractor>
class BasicObjectPool {
 public:
  using Extractor = TIdExtractor;

  static constexpr TComposedId kNullId = Extractor::kNullId;
  static constexpr size_t kNullIndex   = Extractor::kNullIndex;

  explicit BasicObjectPool(std::nullptr_t) {}

  [[nodiscard]] static BasicObjectPool create(size_t max_objs_count) {
    auto obj_pool = BasicObjectPool();

    obj_pool.free_      = std::views::iota(0U, max_objs_count) | std::views::reverse | std::ranges::to<std::vector>();
    obj_pool.obj_count_ = 0;
    obj_pool.version_.resize(max_objs_count, 0);
  }

  [[nodiscard]] TComposedId create() {
    auto ind = free_.back();
    ++obj_count_;
    return TIdExtractor::compose_id(ind, version_[ind]);
  }

  void remove(TComposedId id) {
    --obj_count_;
    auto index = TIdExtractor::index_of(id);
    free_.push_back(index);
    ++version_[index];
  }

  [[nodiscard]] bool exists(TComposedId id) const { return version_[id] == TIdExtractor::version_of(id); }
  [[nodiscard]] size_t count() const { return obj_count_; }

  [[nodiscard]] static size_t index_of(TComposedId id) { return Extractor::index_of(id); }
  [[nodiscard]] static uint32_t version_of(TComposedId id) { return Extractor::version_of(id); }
  [[nodiscard]] static TComposedId compose_id(size_t index, uint32_t version) {
    return Extractor::compose_id(index, version);
  }
  [[nodiscard]] TComposedId compose_id(size_t index) const { return Extractor::compose_id(index, version_[index]); }

 private:
  BasicObjectPool() = default;

  std::vector<uint32_t> version_;
  std::vector<size_t> free_;
  size_t obj_count_{};
};

}  // namespace eray::vkren
