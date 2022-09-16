#include "transform.hlsli"

ConstantBuffer<transform_cb> transform : register(b0);

struct vs_input
{
	float3 position			: POSITION;
	float3 normal			: NORMAL;
};

struct vs_output
{
	float3 worldPosition	: WORLD_POSITION;
	float3 worldNormal		: WORLD_NORMAL;
	float4 position			: SV_POSITION;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	OUT.worldPosition = mul(transform.m, float4(IN.position, 1.f)).xyz;
	OUT.worldNormal = mul(transform.m, float4(IN.normal, 0.f)).xyz;
	return OUT;
}
