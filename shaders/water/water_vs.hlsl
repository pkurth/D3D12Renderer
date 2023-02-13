#include "transform.hlsli"

struct water_cb
{
	float4x4 mvp;
};

ConstantBuffer<water_cb> cb	: register(b0);


float4 main(float3 pos : POSITION) : SV_POSITION
{
	return mul(cb.mvp, float4(pos, 1.f));
}
