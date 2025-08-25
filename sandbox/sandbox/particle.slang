struct Particle {
  float2 position;
  float2 velocity;
  float4 color;
};

struct ParticleSSBO {
  Particle particles;
};
StructuredBuffer<ParticleSSBO> particlesIn;     // read-only
RWStructuredBuffer<ParticleSSBO> particlesOut;  // read-write

struct UniformBuffer {
  float deltaTime;
};
ConstantBuffer<UniformBuffer> ubo;  // read-only

[shader("compute")]
[numthreads(256, 1, 1)]
void mainComp(uint3 threadId: SV_DispatchThreadID) {}
