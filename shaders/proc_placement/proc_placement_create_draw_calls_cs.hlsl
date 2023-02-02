#include "cs.hlsli"
#include "proc_placement_rs.hlsli"

ConstantBuffer<proc_placement_create_draw_calls_cb> cb	: register(b0);
StructuredBuffer<uint> meshCounts						: register(t0);
StructuredBuffer<uint> meshOffsets						: register(t1);
StructuredBuffer<uint> submeshToMesh					: register(t2);
RWStructuredBuffer<placement_draw> draws				: register(u0);


[RootSignature(PROC_PLACEMENT_CREATE_DRAW_CALLS_RS)]
[numthreads(PROC_PLACEMENT_CREATE_DRAW_CALLS_BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	uint id = IN.dispatchThreadID.x;
	uint meshID = submeshToMesh[id];
	draws[id].draw.InstanceCount = meshCounts[meshID];
	draws[id].transformSRV = cb.transformAddress + meshOffsets[meshID] * cb.stride;
}
