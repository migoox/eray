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
RWStructuredBuffer<ParticleSSBO> particles;  // read-write

[shader("compute")]
[numthreads(256, 1, 1)]  // defines the number of invocations of this compute shader in the current work group
void mainComp(uint3 threadId: SV_DispatchThreadID) {
  uint index = threadId.x;

  particles[index].particles.position =
      particles[index].particles.position + particles[index].particles.velocity * ubo.deltaTime;

  // Flip movement at window border
  if ((particles[index].particles.position.x <= -1.0) || (particles[index].particles.position.x >= 1.0)) {
    particles[index].particles.velocity.x = -particles[index].particles.velocity.x;
  }
  if ((particles[index].particles.position.y <= -1.0) || (particles[index].particles.position.y >= 1.0)) {
    particles[index].particles.velocity.y = -particles[index].particles.velocity.y;
  }
}
