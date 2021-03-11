#include "depth_only_rs.hlsli"


ConstantBuffer<point_shadow_transform_cb> transform : register(b0);

struct vs_input
{
	float3 position		: POSITION;
};

struct vs_output
{
	float clipDepth		: CLIP_DEPTH;
	float4 position		: SV_POSITION;
};

[RootSignature(POINT_SHADOW_RS)]
vs_output main(vs_input IN)
{
	float3 position = mul(transform.m, float4(IN.position, 1.f)).xyz;
	
	float3 L = position - transform.lightPosition;

	L.z *= transform.flip;

	float l = length(L);
	L /= l;

	L.xy /= L.z + 1.f;

	vs_output OUT;
	OUT.clipDepth = L.z;
	OUT.position = float4(L.xy, l / transform.maxDistance, 1.f);
	return OUT;
}
