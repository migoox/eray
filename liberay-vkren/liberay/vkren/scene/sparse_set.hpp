#pragma once

#include <concepts>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace eray::vkren {

template <typename TKey, TKey NullKey, typename... TValues>
  requires std::convertible_to<TKey, size_t>
class BasicSparseSet {
 public:
  static BasicSparseSet create(TKey max_key) {
    auto set = BasicSparseSet();
    set.increase_max_key(max_key);
    return set;
  }

  void insert(TKey key, TValues&&... values) {
    if (sparse_.size() <= key) {
      increase_max_key(key);
    }

    assert(dense_.size() <= static_cast<size_t>(std::numeric_limits<TKey>::max()));

    sparse_[static_cast<size_t>(key)] = static_cast<TKey>(dense_.size());
    dense_.push_back(key);

    push_back_values(std::forward<TValues>(values)...);
  }

  void remove(TKey key) {
    auto last_ind = dense_.size() - 1;
    auto curr_ind = sparse_[static_cast<size_t>(key)];
    if (curr_ind == last_ind) {
      values_.pop_back();
      dense_.pop_back();
      sparse_[static_cast<size_t>(key)] = NullKey;
      return;
    }

    sparse_[dense_[curr_ind]]         = curr_ind;
    sparse_[static_cast<size_t>(key)] = NullKey;

    values_[curr_ind] = values_[last_ind];
    std::apply([](auto&... val) { (val.pop_back(), ...); }, values_);

    dense_[curr_ind] = dense_[last_ind];
    dense_.pop_back();
  }

  bool contains(TKey key) const {
    return static_cast<size_t>(key) < sparse_.size() && sparse_[static_cast<size_t>(key)] != NullKey;
  }

  template <typename TValue>
  const TValue& at(TKey key) const {
    assert(contains(key) && "Key does not exist");

    return std::get<TValue>(values_)[sparse_[static_cast<size_t>(key)]];
  }

  template <typename TValue>
  std::optional<TValue> optional_at(TKey key) const {
    if (!contains(key)) {
      return std::nullopt;
    }
    return at(key);
  }

  void increase_max_key(TKey max_key) {
    assert(static_cast<size_t>(max_key) >= sparse_.size());
    sparse_.resize(max_key + 1, NullKey);
  }

  TKey max_key() const { return static_cast<TKey>(sparse_.size() - 1); }

  std::span<TKey> keys() const { return dense_; }

  template <typename TValue>
  std::span<TValue> values() const {
    return std::get<TValue>(values_);
  }

  auto key_value_pairs() const { return std::views::zip(dense_, values_); }

 private:
  template <typename... TArgs>
  void push_back_values(TArgs&&... args) {
    (std::get<std::vector<TArgs>>(values_).push_back(std::forward<TArgs>(args)), ...);
  }

  std::vector<TKey> sparse_;
  std::vector<TKey> dense_;
  std::tuple<std::vector<TValues>...> values_;
};

template <typename TKey, typename... TValues>
using SparseSet = BasicSparseSet<TKey, std::numeric_limits<TKey>::max(), TValues...>;

}  // namespace eray::vkren
