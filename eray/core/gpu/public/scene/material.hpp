#pragma once

#include <liberay/math/mat.hpp>
#include <liberay/math/vec_fwd.hpp>
#include <liberay/util/zstring_view.hpp>
#include <liberay/vkren/scene/entity_pool.hpp>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace eray::vkren {

struct Uniforms {
  std::unordered_map<std::string, TextureId> textures;

  std::unordered_map<std::string, float> float_values;
  std::unordered_map<std::string, math::Vec2f> float2_values;
  std::unordered_map<std::string, math::Vec3f> float3_values;
  std::unordered_map<std::string, math::Vec4f> float4_values;

  std::unordered_map<std::string, int> int_values;
  std::unordered_map<std::string, math::Vec2i> int2_values;
  std::unordered_map<std::string, math::Vec3i> int3_values;
  std::unordered_map<std::string, math::Vec4i> int4_values;

  std::unordered_map<std::string, math::Mat4f> mat_values;

  template <typename TType>
  TType get(util::zstring_view name) const {
    if constexpr (std::is_same_v<TType, TextureId>) {
      return textures.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, float>) {
      return float_values.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, math::Vec2f>) {
      return float2_values.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, math::Vec3f>) {
      return float3_values.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, math::Vec4f>) {
      return float4_values.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, int>) {
      return int_values.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, math::Vec2i>) {
      return int2_values.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, math::Vec3i>) {
      return int3_values.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, math::Vec4i>) {
      return int4_values.at(name.c_str());
    } else if constexpr (std::is_same_v<TType, math::Mat4f>) {
      return mat_values.at(name.c_str());
    } else {
      static_assert([] { return false; }(), "Unsupported Uniform type for get()");
    }
  }
};

struct Material {
  enum class Info : uint8_t { PBR, Custom } info{};
  Uniforms uniform_data;
};

struct GPUMaterial {
  vk::Pipeline pipeline;
  vk::PipelineLayout layout;
  vk::DescriptorSet material_set;
};

}  // namespace eray::vkren
