#include "ubo.slang"
#include "phong.slang"

struct VSInput {
  float3 pos;
  float3 normal;
  float3 color;
};

struct VertexOutput {
  float4 pos : SV_Position;
  float3 fragPos : POSITION;
  float3 normal : NORMAL;
  float2 texCoords : TEXCOORDS;
  float3 color : COLOR;
};

ConstantBuffer<UniformBuffer> ubo;

[shader("vertex")]
VertexOutput mainVert(VSInput input, uint instance_id: SV_InstanceID) {
  VertexOutput output;
  output.pos     = float4(input.pos, 1.0);
  output.pos     = mul(ubo.model, output.pos);
  output.fragPos = input.pos.xyz;
  output.pos     = mul(ubo.view, output.pos);
  output.pos     = mul(ubo.proj, output.pos);
  output.normal  = mul(ubo.model, float4(input.normal, 0.)).xyz;
  output.color   = input.color;
  return output;
}

[shader("fragment")]
float4 mainFrag(VertexOutput input) : SV_Target {
  return float4(phongDirLight(input.color, ubo.lightDir.xyz, ubo.cameraPos.xyz, input.fragPos, input.normal), 1.);
}
