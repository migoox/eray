#pragma once

#include <concepts>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace eray::vkren {

template <typename TKey, typename TValueType, TKey NullKey>
  requires std::convertible_to<TKey, size_t>
class SparseSet {
 public:
  static SparseSet create(TKey max_key) {
    auto set = SparseSet();
    set.increase_max_key(max_key);
    return set;
  }

  void insert(TKey key, TValueType value) {
    if (sparse_.size() <= key) {
      increase_max_key(key);
    }

    assert(dense_.size() <= static_cast<size_t>(std::numeric_limits<TKey>::max()));

    sparse_[static_cast<size_t>(key)] = static_cast<TKey>(dense_.size());
    dense_.push_back(key);
    values_.push_back(value);
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
    values_.pop_back();

    dense_[curr_ind] = dense_[last_ind];
    dense_.pop_back();
  }

  bool contains(TKey key) {
    return static_cast<size_t>(key) < sparse_.size() && sparse_[static_cast<size_t>(key)] != NullKey;
  }

  const TValueType& at(TKey key) {
    assert(contains(key) && "Key does not exist");

    return values_[sparse_[static_cast<size_t>(key)]];
  }

  std::optional<TValueType> optional_at(TKey key) {
    if (!contains(key)) {
      return std::nullopt;
    }
    return at(key);
  }

  void increase_max_key(TKey max_key) {
    assert(static_cast<size_t>(max_key) >= sparse_.size());
    sparse_.resize(max_key + 1, NullKey);
  }

  TKey max_key() { return static_cast<TKey>(sparse_.size() - 1); }

  std::span<TValueType> keys() { return dense_; }
  std::span<TValueType> values() { return values_; }
  auto key_value_pairs() { return std::views::zip(dense_, values_); }

 private:
  std::vector<TKey> sparse_;
  std::vector<TKey> dense_;
  std::vector<TValueType> values_;
};

}  // namespace eray::vkren
