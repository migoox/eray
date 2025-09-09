#pragma once

#include <liberay/res/shader.hpp>
#include <liberay/vkren/common.hpp>
#include <liberay/vkren/device.hpp>

namespace eray::vkren {

struct ShaderModule {
  vk::raii::ShaderModule shader_module;
  observer_ptr<const Device> _p_device;

  /**
   * @brief Creates a shader module from the provided `bytecode` span.
   *
   * @param device
   * @param bytecode
   * @return Result<ShaderModule, Error>
   */
  [[nodiscard]] static Result<ShaderModule, Error> create(const Device& device,
                                                          std::span<const uint32_t> spirv_bytecode);
  [[nodiscard]] static Result<ShaderModule, Error> create(const Device& device, const res::SPIRVShaderBinary& spirv);
};

}  // namespace eray::vkren
