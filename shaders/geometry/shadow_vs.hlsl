#include "depth_only_rs.hlsli"


struct light_vp
{
	float4x4 viewProj;
};

StructuredBuffer<float4x4> transforms	: register(t0);
ConstantBuffer<light_vp> vp				: register(b0);

struct vs_input
{
	float3 position		: POSITION;

	uint instanceID		: SV_InstanceID;
};

struct vs_output
{
	float4 position			: SV_POSITION;
};

[RootSignature(SHADOW_RS)]
vs_output main(vs_input IN)
{
	float4 worldPosition = mul(transforms[IN.instanceID], float4(IN.position, 1.f));

	vs_output OUT;
	OUT.position = mul(vp.viewProj, worldPosition);
	return OUT;
}
