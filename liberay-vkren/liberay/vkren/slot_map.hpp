#pragma once
#include <liberay/vkren/handle.hpp>
#include <vector>

namespace eray::vkren {

template <HandleConcept THandle, typename TEntry>
class SlotMap {
 public:
  SlotMap()                        = default;
  SlotMap(SlotMap&&) noexcept      = default;
  SlotMap(const SlotMap&) noexcept = delete;

  SlotMap& operator=(SlotMap&&)      = delete;
  SlotMap& operator=(const SlotMap&) = delete;

  ~SlotMap() { reset(); }

  constexpr static uint32_t kChunkSize = 256;

  THandle create_entry(const TEntry& entry) {
    if (free_.empty()) {
      TEntry* chunk = new TEntry[kChunkSize];
      for (size_t i = 0; i < kChunkSize; ++i) {
        free_.push_back(entries_table_.size() * kChunkSize + static_cast<THandle::IndexType>(i));
        versions_.push_back(0);
      }
      entries_table_.push_back(chunk);
    }

    THandle::IndexType free_index = free_.back();
    free_.pop_back();
    entries_table_[free_index / kChunkSize][free_index % kChunkSize] = entry;
    return THandle::create(free_index, versions_[free_index]);
  }

  bool is_valid(THandle handle) { return handle.index() < versions_.size() && versions_[handle.index()] == handle.version(); }

  TEntry* get_entry(THandle handle) {
    if (handle.index() >= versions_.size() || versions_[handle.index()] != handle.version()) {
      return nullptr;
    }
    return &entries_table_[handle.index() / kChunkSize][handle.index() % kChunkSize];
  }

  const TEntry* get_entry(THandle handle) const {
    if (handle.index() >= versions_.size() || versions_[handle.index()] != handle.version()) {
      return nullptr;
    }
    return &entries_table_[handle.index() / kChunkSize][handle.index() % kChunkSize];
  }

  void delete_entry(THandle handle) {
    if (handle.index() >= versions_.size() || versions_[handle.index()] != handle.version()) {
      return;
    }
    versions_[handle.index()]++;
    free_.push_back(handle.index());
  }

  void reset() {
    free_.clear();
    versions_.clear();
    for (TEntry* chunk : entries_table_) {
      delete[] chunk;
    }
    entries_table_.clear();
  }

 private:
  std::vector<THandle::IndexType> free_;
  std::vector<THandle::IndexType> versions_;
  std::vector<TEntry*> entries_table_;
};

}  // namespace eray::vkren