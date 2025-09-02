#include <expected>
#include <liberay/vkren/error.hpp>
#include <liberay/vkren/shader.hpp>

namespace eray::vkren {

Result<ShaderModule, Error> ShaderModule::create(const Device& device, std::span<const uint32_t> spirv_bytecode) {
  auto module_info = vk::ShaderModuleCreateInfo{
      .codeSize = spirv_bytecode.size() * sizeof(uint32_t),
      .pCode    = spirv_bytecode.data(),
  };

  // Shader modules are a thin wrapper around the shader bytecode.
  auto shader_mod_opt = device->createShaderModule(module_info);
  if (!shader_mod_opt) {
    eray::util::Logger::err("Failed to create a shader module");
    return std::unexpected(Error{
        .msg     = "Shader Module creation failure",
        .code    = ErrorCode::VulkanObjectCreationFailure{},
        .vk_code = shader_mod_opt.error(),
    });
  }
  return ShaderModule{
      .shader_module = std::move(*shader_mod_opt),
      .p_device      = &device,
  };
}

Result<ShaderModule, Error> ShaderModule::create(const Device& device, const res::SPIRVShaderBinary& spirv) {
  return ShaderModule::create(device, spirv.data());
}

}  // namespace eray::vkren
