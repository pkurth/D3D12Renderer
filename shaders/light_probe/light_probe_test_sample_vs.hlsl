#include "transform.hlsli"

ConstantBuffer<transform_cb> transform	: register(b0);

struct vs_input
{
	float3 position		: POSITION;
};

struct vs_output
{
	float3 worldPosition	: POSITION;
	float3 worldNormal		: NORMAL;
	float4 position			: SV_POSITION;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.worldPosition = mul(transform.m, float4(IN.position, 1.f));
	OUT.worldNormal = mul(transform.m, float4(IN.position, 0.f));
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	return OUT;
}
