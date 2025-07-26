// Something worth mentioning if you’re familiar with other shading languages like GLSL or HLSL, there are no
// instructions for bindings. This is a feature of Slang. Slang is designed to automatically infer the bindings by the
// order of declaration

// The struct for positions is a static to inform the compiler that we don’t need any bindings in our shader.
static float2 positions[3] = float2[](  //
    float2(0.0, -0.5),                  //
    float2(0.5, 0.5),                   //
    float2(-0.5, 0.5)                   //
);

static float3 colors[3] = float3[](  //
    float3(1.0, 0.0, 0.0),           //
    float3(0.0, 1.0, 0.0),           //
    float3(0.0, 0.0, 1.0),           //
);

struct VertexOutput {
  float3 color;
  float4 sv_position : SV_Position;
};

// Slang and SPIR-V both support having multiple entry points in one file. This means
// 2 entry points for vertex and fragment shaders.
[shader("vertex")]
VertexOutput mainVert(uint vid: SV_VertexID) {
  VertexOutput output;
  output.sv_position = float4(positions[vid], 0.0, 1.0);
  output.color       = colors[vid];
  return output;
}

[shader("fragment")]
float4 mainFrag(VertexOutput inVert) : SV_Target {
  return float4(inVert.color, 1.0);
}

