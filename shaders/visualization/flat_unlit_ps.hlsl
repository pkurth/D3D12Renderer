#include "visualization_rs.hlsli"

ConstantBuffer<visualization_cb> cb		: register(b1);



struct ps_input
{
	float3 color		: COLOR;
};

[RootSignature(FLAT_UNLIT_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return cb.color * float4(IN.color, 1.f);
}
