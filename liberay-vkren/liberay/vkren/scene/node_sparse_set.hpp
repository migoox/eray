#pragma once

#include <liberay/vkren/scene/flat_tree.hpp>
#include <liberay/vkren/scene/sparse_set.hpp>

namespace eray::vkren {

template <typename TValueType>

using NodeSparseSet = SparseSet<NodeIndex, TValueType, FlatTree::kNullNodeIndex>();

}
