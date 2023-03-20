#include "depth_only_rs.hlsli"


StructuredBuffer<float4x4> transforms	: register(t0);
ConstantBuffer<point_shadow_cb> cb		: register(b0);

struct vs_input
{
	float3 position		: POSITION;

	uint instanceID		: SV_InstanceID;
};

struct vs_output
{
	float clipDepth		: CLIP_DEPTH;
	float4 position		: SV_POSITION;
};

[RootSignature(POINT_SHADOW_RS)]
vs_output main(vs_input IN)
{
	float3 worldPosition = mul(transforms[IN.instanceID], float4(IN.position, 1.f)).xyz;
	
	float3 L = worldPosition - cb.lightPosition;

	L.z *= cb.flip;

	float l = length(L);
	L /= l;

	L.xy /= L.z + 1.f;

	vs_output OUT;
	OUT.clipDepth = L.z;
	OUT.position = float4(L.xy, l / cb.maxDistance, 1.f);
	return OUT;
}
