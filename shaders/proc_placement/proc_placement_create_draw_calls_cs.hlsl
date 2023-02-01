#include "cs.hlsli"
#include "proc_placement_rs.hlsli"

StructuredBuffer<uint> meshCounts			: register(t0);
StructuredBuffer<uint> submeshToMesh		: register(t1);
RWStructuredBuffer<placement_draw> draws	: register(u0);


[RootSignature(PROC_PLACEMENT_CREATE_DRAW_CALLS_RS)]
[numthreads(PROC_PLACEMENT_CREATE_DRAW_CALLS_BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	uint id = IN.dispatchThreadID.x;
	draws[id].draw.InstanceCount = meshCounts[submeshToMesh[id]];
}

