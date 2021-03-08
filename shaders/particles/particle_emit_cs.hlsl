#include "cs.hlsli"
#include "particles_rs.hlsli"

RWStructuredBuffer<particle_draw> drawInfo				: register(u1);
RWStructuredBuffer<particle_counters> counters			: register(u2);

RWStructuredBuffer<particle_data> particles				: register(u3);
RWStructuredBuffer<uint> deadList						: register(u4);
RWStructuredBuffer<uint> currentAliveList				: register(u5);


static particle_data emitParticle(uint rng)
{
	particle_data particle = {
		float3(rng * 3, 0.f, 0.f),
		5.f,
		float3(0.f, 0.f, 0.f),
		rng
	};

	return particle;
}

[numthreads(PARTICLES_EMIT_BLOCK_SIZE, 1, 1)]
[RootSignature(PARTICLES_COMPUTE_RS)]
void main(cs_input IN)
{
	const uint i = IN.dispatchThreadID.x;
	const uint count = counters[0].newParticles;
	if (i >= count)
	{
		return;
	}

	uint dead;
	InterlockedAdd(counters[0].numDeadParticles, -1, dead);
	
	uint index = deadList[dead - 1];
	particles[index] = emitParticle(i);

	uint alive;
	InterlockedAdd(counters[0].numAliveParticlesThisFrame, 1, alive);

	currentAliveList[alive] = index;
}
