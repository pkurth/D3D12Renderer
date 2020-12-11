#include "flat_simple_rs.hlsli"

ConstantBuffer<flat_simple_transform_cb> transform : register(b0);

struct vs_input
{
	float3 position		: POSITION;
	float3 normal		: NORMAL;
};

struct vs_output
{
	float3 viewPosition	: VIEW_POSITION;
	float3 normal		: NORMAL;
	float4 position		: SV_POSITION;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	OUT.viewPosition = mul(transform.mv, float4(IN.position, 1.f)).xyz;
	OUT.normal = mul(transform.mv, float4(IN.normal, 0.f)).xyz;
	return OUT;
}
