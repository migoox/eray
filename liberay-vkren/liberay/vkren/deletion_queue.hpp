#pragma once

#include <deque>
#include <functional>
#include <ranges>

namespace eray::vkren {

class DeletionQueue {
  DeletionQueue() = default;
  static DeletionQueue create() { return DeletionQueue(); }

  /**
   * @brief Push deletor callback.
   *
   * @param function
   */
  void push_deletor(std::function<void()>&& function) { deletors_.emplace_back(std::move(function)); }

  /**
   * @brief Invokes all the deletors. The last added deletor is invoked first.
   *
   */
  void flush() {
    for (auto& deletor : std::ranges::reverse_view(deletors_)) {
      deletor();
    }
    deletors_.clear();
  }

  const std::deque<std::function<void()>>& deletors() const { return deletors_; }

 private:
  // NOTE: Doing callbacks like this is inneficient at scale, because we are storing whole std::functions for every
  // object we are deleting, which is not going to be optimal. For the amount of objects we will use in this tutorial,
  // its going to be fine. but if you need to delete thousands of objects and want them deleted faster, a better
  // implementation would be to store arrays of vulkan handles of various types such as VkImage, VkBuffer, and so on.
  // And then delete those from a loop.

  std::deque<std::function<void()>> deletors_;
};

}  // namespace eray::vkren
