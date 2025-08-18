// Something worth mentioning if you’re familiar with other shading languages like GLSL or HLSL, there are no
// instructions for bindings. This is a feature of Slang. Slang is designed to automatically infer the bindings by the
// order of declaration

struct VSInput {
  // vertex attributes
  float2 pos;
  float3 color;
  float2 texCoords;
};

struct UniformBuffer {
  float4x4 model;
  float4x4 view;
  float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

struct VertexOutput {
  float4 pos : SV_Position;
  float3 color;
  float2 fragTexCoords;
};

// Slang and SPIR-V both support having multiple entry points in one file. This means
// 2 entry points for vertex and fragment shaders.
[shader("vertex")]
VertexOutput mainVert(VSInput input) {
  VertexOutput output;
  output.color         = input.color;
  output.pos           = mul(ubo.model, float4(input.pos, 1.0));
  output.pos           = mul(ubo.view, output.pos);
  output.pos           = mul(ubo.proj, output.pos);
  output.fragTexCoords = input.texCoords;
  return output;
}

Sampler2D texture;

[shader("fragment")]
float4 mainFrag(VertexOutput input) : SV_Target {
  return float4(texture.Sample(input.fragTexCoords).rgb * input.color, 1.0);
}

