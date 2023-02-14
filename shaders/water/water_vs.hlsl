#include "water_rs.hlsli"

ConstantBuffer<water_transform_cb> cb	: register(b0);


float4 main(float3 pos : POSITION) : SV_POSITION
{
	return mul(cb.mvp, float4(pos, 1.f));
}
