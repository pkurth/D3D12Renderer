#include "cs.hlsli"
#include "particles_rs.hlsli"
#include "random.hlsli"

ConstantBuffer<particles_sim_cb> cb						: register(b0);

RWStructuredBuffer<particle_draw> drawInfo				: register(u1);
RWStructuredBuffer<particle_counters> counters			: register(u2);

RWStructuredBuffer<particle_data> particles				: register(u3);
RWStructuredBuffer<uint> deadList						: register(u4);
RWStructuredBuffer<uint> currentAliveList				: register(u5);


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

	uint rand = initRand(i, cb.frameIndex);
	particles[index] = emitParticle(rand);

	uint alive;
	InterlockedAdd(counters[0].numAliveParticlesThisFrame, 1, alive);

	currentAliveList[alive] = index;
}
