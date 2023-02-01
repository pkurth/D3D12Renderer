#include "cs.hlsli"
#include "proc_placement_rs.hlsli"

StructuredBuffer<placement_point> placementPoints	: register(t0);
StructuredBuffer<uint> meshOffsets					: register(t1);

RWStructuredBuffer<placement_transform> transforms	: register(u0);
RWStructuredBuffer<uint> meshCounts					: register(u1);

[RootSignature(PROC_PLACEMENT_CREATE_TRANSFORMS_RS)]
[numthreads(PROC_PLACEMENT_CREATE_TRANSFORMS_BLOCK_SIZE, 1, 1)]
void main(cs_input IN)
{
	uint totalCount = meshCounts[0];

	uint id = IN.dispatchThreadID.x;
	if (id >= totalCount)
	{
		return;
	}

	placement_point p = placementPoints[id];
	
	uint globalOffset = meshOffsets[p.meshID];
	
	uint localOffset;
	InterlockedAdd(meshCounts[p.meshID], -1, localOffset);
	--localOffset;
	
	float3 yAxis = p.normal;
	float3 xAxis = normalize(cross(yAxis, float3(0.f, 0.f, 1.f)));
	float3 zAxis = cross(xAxis, yAxis);

	float4x4 m = {
		{ xAxis.x, yAxis.x, zAxis.x, p.position.x },
		{ xAxis.y, yAxis.y, zAxis.y, p.position.y },
		{ xAxis.z, yAxis.z, zAxis.z, p.position.z },
		{ 0, 0, 0, 1 }
	};

	placement_transform transform;
	transform.m = m;
	
	transforms[globalOffset + localOffset] = transform;
}
