#include "present_rs.hlsl"

ConstantBuffer<tonemap_cb> tonemap : register(b0);

struct ps_input
{
	float2 uv	: TEXCOORDS;
};

[RootSignature(PRESENT_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return float4(IN.uv * tonemap.exposure, 0.f, 1.f);
}
