#include "depth_only_rs.hlsli"


ConstantBuffer<shadow_transform_cb> transform : register(b0);

struct vs_input
{
	float3 position		: POSITION;
};

struct vs_output
{
	float4 position			: SV_POSITION;
};

[RootSignature(SHADOW_RS)]
vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	return OUT;
}
