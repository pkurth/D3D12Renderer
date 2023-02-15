#include "water_rs.hlsli"

ConstantBuffer<transform_cb> cb	: register(b0);


struct vs_output
{
	float3 worldPosition	: POSITION;

	float4 position	: SV_POSITION;
};

vs_output main(float3 pos : POSITION)
{
	vs_output OUT;
	OUT.worldPosition = mul(cb.m, float4(pos, 1.f));
	OUT.position = mul(cb.mvp, float4(pos, 1.f));
	return OUT;
}
