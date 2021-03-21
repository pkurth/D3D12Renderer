#include "cs.hlsli"
#include "particles_rs.hlsli"

ConstantBuffer<particle_sim_cb> simCB					: register(b0, space1);

RWStructuredBuffer<particle_draw> drawInfo				: register(u1, space1);
RWStructuredBuffer<particle_counters> counters			: register(u2, space1);

RWStructuredBuffer<particle_data> particles				: register(u3, space1);
RWStructuredBuffer<uint> deadList						: register(u4, space1);
RWStructuredBuffer<uint> currentAliveList				: register(u5, space1);
RWStructuredBuffer<uint> newAliveList					: register(u6, space1);

#ifdef REQUIRES_SORTING
RWStructuredBuffer<float> sortingList					: register(u7, space1);
#endif



[numthreads(PARTICLES_SIMULATE_BLOCK_SIZE, 1, 1)]
[RootSignature(PARTICLE_COMPUTE_RS)]
void main(cs_input IN)
{
	const uint i = IN.dispatchThreadID.x;
	const uint count = counters[0].numAliveParticlesThisFrame;
	if (i >= count)
	{
		return;
	}

	float dt = simCB.dt;

	uint index = currentAliveList[i];

	particle_data particle = particles[index];

#ifdef REQUIRES_SORTING
	float sortKey;
	bool shouldLive = simulateParticle(particle, dt, sortKey);
#else
	bool shouldLive = simulateParticle(particle, dt);
#endif

	if (shouldLive)
	{
		particles[index] = particle;

		uint alive;
		InterlockedAdd(drawInfo[0].arguments.InstanceCount, 1, alive);
		newAliveList[alive] = index;

#ifdef REQUIRES_SORTING
		sortingList[alive] = sortKey;
#endif
	}
	else
	{
		uint dead;
		InterlockedAdd(counters[0].numDeadParticles, 1, dead);
		deadList[dead] = index;
	}
}
