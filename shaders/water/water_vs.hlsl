#include "water_rs.hlsli"

ConstantBuffer<transform_cb> cb	: register(b0);


struct vs_output
{
	float3 worldPosition	: POSITION;

	float4 position	: SV_POSITION;
};

vs_output main(uint vertexID : SV_VertexID)
{
	float3 pos = float3(vertexID & 1, 0.5f, vertexID >> 1) * 2.f - 1.f;

	vs_output OUT;
	OUT.worldPosition = mul(cb.m, float4(pos, 1.f));
	OUT.position = mul(cb.mvp, float4(pos, 1.f));
	return OUT;
}
