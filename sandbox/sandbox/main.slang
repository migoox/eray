// Something worth mentioning if you’re familiar with other shading languages like GLSL or HLSL, there are no
// instructions for bindings. This is a feature of Slang. Slang is designed to automatically infer the bindings by the
// order of declaration

struct VSInput {
  // vertex attributes
  float2 pos;
  float3 color;
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
};

// Slang and SPIR-V both support having multiple entry points in one file. This means
// 2 entry points for vertex and fragment shaders.
[shader("vertex")]
VertexOutput mainVert(VSInput input) {
  VertexOutput output;
  output.color = input.color;
  output.pos   = mul(ubo.model, float4(input.pos, 1.0));
  output.pos   = mul(ubo.view, output.pos);
  output.pos   = mul(ubo.proj, output.pos);
  return output;
}

[shader("fragment")]
float4 mainFrag(VertexOutput inVert) : SV_Target {
  return float4(inVert.color, 1.0);
}

