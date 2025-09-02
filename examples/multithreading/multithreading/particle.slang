struct PushConstants {
  uint startIndex;
  uint count;
};

struct Particle {
  float2 position;
  float2 velocity;
  float4 color;
};
struct UniformBuffer {
  float deltaTime;
};
struct ParticleSSBO {
  Particle particles;
};
[[vk::binding(0, 0)]]
ConstantBuffer<UniformBuffer> ubo;  // read-only

[[vk::push_constant]]
PushConstants pushConstants;

[[vk::binding(1, 0)]]
StructuredBuffer<ParticleSSBO> particlesIn;  // read-only

[[vk::binding(2, 0)]]
RWStructuredBuffer<ParticleSSBO> particlesOut;  // read-write

[shader("compute")]
[numthreads(256, 1, 1)]  // defines the number of invocations of this compute shader in the current work group
void mainComp(uint3 threadId: SV_DispatchThreadID) {
  uint local_index = threadId.x;

  if (local_index >= pushConstants.count) {
    return;
  }

  uint index = pushConstants.startIndex + local_index;

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
