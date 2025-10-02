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

    obj_pool.free_ =
        std::views::iota(0U, max_objs_count) | std::views::reverse | std::ranges::to<std::vector<size_t>>();
    obj_pool.obj_count_ = 0;
    obj_pool.version_.resize(max_objs_count, 0);
    obj_pool.exist_.resize(max_objs_count, false);

    return obj_pool;
  }

  [[nodiscard]] TComposedId create() {
    auto ind = free_.back();
    free_.pop_back();
    ++obj_count_;
    exist_[ind] = true;
    return TIdExtractor::compose_id(ind, version_[ind]);
  }

  void remove(TComposedId id) {
    --obj_count_;
    auto index    = TIdExtractor::index_of(id);
    exist_[index] = false;
    free_.push_back(index);
    ++version_[index];
  }

  [[nodiscard]] bool exists(TComposedId id) const {
    auto index   = TIdExtractor::index_of(id);
    auto version = TIdExtractor::version_of(id);
    return index < exist_.size() && exist_[index] && version_[index] == version;
  }

  [[nodiscard]] size_t count() const { return obj_count_; }

  /**
   * @brief Returns null id if object indexed with `index` does not exist.
   *
   * @param index
   * @return TComposedId
   */
  [[nodiscard]] TComposedId compose_id(size_t index) const {
    if (index == kNullIndex || index >= exist_.size() || !exist_[index]) {
      return kNullId;
    }

    return Extractor::compose_id(index, version_[index]);
  }

  [[nodiscard]] static size_t index_of(TComposedId id) { return Extractor::index_of(id); }
  [[nodiscard]] static uint32_t version_of(TComposedId id) { return Extractor::version_of(id); }
  [[nodiscard]] static TComposedId compose_id(size_t index, uint32_t version) {
    return Extractor::compose_id(index, version);
  }

 private:
  BasicObjectPool() = default;

  std::vector<uint32_t> version_;
  std::vector<bool> exist_;
  std::vector<size_t> free_;
  size_t obj_count_{};
};

}  // namespace eray::vkren
