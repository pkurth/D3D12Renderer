#include "cs.hlsli"
#include "grass_rs.hlsli"

ConstantBuffer<grass_create_draw_calls_cb> cb	: register(b0);
StructuredBuffer<uint> counts					: register(t0);
RWStructuredBuffer<grass_draw> draws			: register(u0);


[RootSignature(GRASS_CREATE_DRAW_CALLS_RS)]
[numthreads(GRASS_CREATE_DRAW_CALLS_BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	uint id = IN.dispatchThreadID.x;
	draws[id].draw.InstanceCount = min(counts[id], cb.maxNumInstances);
}
