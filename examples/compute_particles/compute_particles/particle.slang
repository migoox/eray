struct Particle {
  float2 position;
  float2 velocity;
  float4 color;
};
struct UniformBuffer {
  float deltaTime;
};
ConstantBuffer<UniformBuffer> ubo;  // read-only
struct ParticleSSBO {
  Particle particles;
};
StructuredBuffer<ParticleSSBO> particlesIn;     // read-only
RWStructuredBuffer<ParticleSSBO> particlesOut;  // read-write

[shader("compute")]
[numthreads(256, 1, 1)]  // defines the number of invocations of this compute shader in the current work group
void mainComp(uint3 threadId: SV_DispatchThreadID) {
  uint index = threadId.x;

  particlesOut[index].particles.position =
      particlesIn[index].particles.position + particlesIn[index].particles.velocity * ubo.deltaTime;
  particlesOut[index].particles.velocity = particlesIn[index].particles.velocity;

  // Flip movement at window border
  if ((particlesOut[index].particles.position.x <= -1.0) || (particlesOut[index].particles.position.x >= 1.0)) {
    particlesOut[index].particles.velocity.x = -particlesOut[index].particles.velocity.x;
  }
  if ((particlesOut[index].particles.position.y <= -1.0) || (particlesOut[index].particles.position.y >= 1.0)) {
    particlesOut[index].particles.velocity.y = -particlesOut[index].particles.velocity.y;
  }
}
