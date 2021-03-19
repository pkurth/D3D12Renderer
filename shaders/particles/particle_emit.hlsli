#include "cs.hlsli"
#include "particles_rs.hlsli"

ConstantBuffer<particle_sim_cb> simCB					: register(b0, space1);

RWStructuredBuffer<particle_draw> drawInfo				: register(u1, space1);
RWStructuredBuffer<particle_counters> counters			: register(u2, space1);

RWStructuredBuffer<particle_data> particles				: register(u3, space1);
RWStructuredBuffer<uint> deadList						: register(u4, space1);
RWStructuredBuffer<uint> currentAliveList				: register(u5, space1);


[numthreads(PARTICLES_EMIT_BLOCK_SIZE, 1, 1)]
[RootSignature(PARTICLE_COMPUTE_RS)]
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
