#pragma once
#include <vector>

namespace eray::vkren {

using ResourceHandleIndex   = uint32_t;
using ResourceHandleVersion = uint32_t;

static const ResourceHandleIndex kInvalidResourceHandleIndex = 0xFFFFFFFF;

template <typename T>
struct ResourceHandle {
  ResourceHandleIndex index;
  ResourceHandleVersion version;

  static constexpr ResourceHandle<T> kInvalid = ResourceHandle<T>{.index = kInvalidResourceHandleIndex, .version = 0};
};

using BufferHandle  = ResourceHandle<struct BufferTag>;
using ImageHandle   = ResourceHandle<struct ImageTag>;
using SamplerHandle = ResourceHandle<struct SamplerTag>;

template <typename THandle, typename TEntry>
class SlotMap {
 public:
  SlotMap()                          = default;
  SlotMap(SlotMap&&) noexcept        = default;
  SlotMap(const SlotMap&) noexcept   = delete;

  SlotMap& operator=(SlotMap&&)      = delete;
  SlotMap& operator=(const SlotMap&) = delete;
  
  ~SlotMap() { reset(); }

  constexpr static uint32_t kChunkSize = 256;

  THandle create_entry(const TEntry& entry) {
    if (free_.empty()) {
      TEntry* chunk = new TEntry[kChunkSize];
      for (size_t i = 0; i < kChunkSize; ++i) {
        free_.push_back(entries_table_.size() * kChunkSize + static_cast<ResourceHandleIndex>(i));
        versions_.push_back(0);
      }
      entries_table_.push_back(chunk);
    }

    ResourceHandleIndex free_index = free_.back();
    free_.pop_back();
    entries_table_[free_index / kChunkSize][free_index % kChunkSize] = entry;
    return THandle{
        .index   = free_index,
        .version = versions_[free_index],
    };
  }

  TEntry* get_entry(THandle handle) {
    if (handle.index >= versions_.size() || versions_[handle.index] != handle.version) {
      return nullptr;
    }
    return &entries_table_[handle.index / kChunkSize][handle.index % kChunkSize];
  }

  const TEntry* get_entry(THandle handle) const {
    if (handle.index >= versions_.size() || versions_[handle.index] != handle.version) {
      return nullptr;
    }
    return &entries_table_[handle.index / kChunkSize][handle.index % kChunkSize];
  }

  void delete_entry(THandle handle) {
    if (handle.index >= versions_.size() || versions_[handle.index] != handle.version) {
      return;
    }
    versions_[handle.index]++;
    free_.push_back(handle.index);
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
  std::vector<ResourceHandleIndex> free_;
  std::vector<ResourceHandleVersion> versions_;
  std::vector<TEntry*> entries_table_;
};

}  // namespace eray::vkren