#include "cs.hlsli"
#include "particles_rs.hlsli"

RWStructuredBuffer<particle_dispatch> dispatchInfo		: register(u0);
RWStructuredBuffer<particle_draw> drawInfo				: register(u1);
RWStructuredBuffer<particle_counters> counters			: register(u2);


static uint bucketize(uint problemSize, uint bucketSize) 
{ 
	return (problemSize + bucketSize - 1) / bucketSize; 
}


[numthreads(1, 1, 1)]
[RootSignature(PARTICLES_COMPUTE_RS)]
void main(cs_input IN)
{
	particle_counters c = counters[0];
	particle_draw draw = drawInfo[0];
	particle_dispatch dispatch = dispatchInfo[0];

	float dt = 1.f / 60.f;
	c.emitRateAccum += 200 * dt; // TODO: How do we prevent this from running to infinity, if the system is saturated?

	uint spaceLeft = c.numDeadParticles;
	uint numNewParticles = min((uint)c.emitRateAccum, spaceLeft);

	c.emitRateAccum -= numNewParticles;

	uint numAliveParticlesInLastFrame = draw.arguments.InstanceCount;

	dispatch.emit.ThreadGroupCountX = bucketize(numNewParticles, PARTICLES_EMIT_BLOCK_SIZE);
	dispatch.emit.ThreadGroupCountY = 1;
	dispatch.emit.ThreadGroupCountZ = 1;

	dispatch.simulate.ThreadGroupCountX = bucketize(numAliveParticlesInLastFrame + numNewParticles, PARTICLES_SIMULATE_BLOCK_SIZE);
	dispatch.simulate.ThreadGroupCountY = 1;
	dispatch.simulate.ThreadGroupCountZ = 1;

	c.numAliveParticlesThisFrame = numAliveParticlesInLastFrame;
	draw.arguments.InstanceCount = 0;
	c.newParticles = numNewParticles;

	dispatchInfo[0] = dispatch;
	drawInfo[0] = draw;
	counters[0] = c;
}
