struct VertexInput {
  float2 pos;
  float4 color;
};

struct VertexOutput {
  float4 pos : SV_Position;
  float pointSize : SV_PointSize;
  float4 color;
};

// Slang and SPIR-V both support having multiple entry points in one file. This means
// 2 entry points for vertex and fragment shaders.
[shader("vertex")]
VertexOutput mainVert(VertexInput input) {
  VertexOutput output;
  output.pos       = float4(input.pos, 0.5, 1.0);
  output.color     = input.color;
  output.pointSize = 14.0;
  return output;
}

[shader("fragment")]
float4 mainFrag(VertexOutput input) : SV_Target {
  return input.color;
}

