#include "grass_rs.hlsli"

struct ps_input
{
	float2 uv		: TEXCOORDS;
};

[RootSignature(GRASS_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return float4(0.f, 1.f, 0.f, 1.f);
}

