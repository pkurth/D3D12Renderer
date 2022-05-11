#include "light_probe_rs.hlsli"

ConstantBuffer<light_probe_ray_visualization_cb> cb : register(b0);
Texture2D<float3> radiance							: register(t0);
Texture2D<float4> directionAndDistance				: register(t1);

struct vs_input
{
	uint vertexID		: SV_VertexID;
	uint instanceID		: SV_InstanceID;
};

struct vs_output
{
	float3 color			: COLOR;
	float distance			: DISTANCE;
	float4 position			: SV_POSITION;
};

vs_output main(vs_input IN)
{
	uint probeID = IN.instanceID / NUM_RAYS_PER_PROBE;
	uint rayID = IN.instanceID % NUM_RAYS_PER_PROBE;

	uint2 c = uint2(rayID, probeID);

	float4 dirDist = directionAndDistance[c];

	float3 index = linearIndexTo3DIndex(probeID, cb.countX, cb.countY);
	float3 position = index * cb.cellSize;

	if (IN.vertexID == 1)
	{
		position += dirDist.xyz * dirDist.w;
	}

	vs_output OUT;
	OUT.position = mul(cb.mvp, float4(position, 1.f));
	OUT.color = radiance[c];
	OUT.distance = dirDist.w;
	return OUT;
}
