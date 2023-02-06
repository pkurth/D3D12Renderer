#include "grass_rs.hlsli"

struct ps_input
{
	float2 uv		: TEXCOORDS;
};

[RootSignature(GRASS_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return float4(
		lerp(pow(float3(121, 208, 33) / 255, 2.2), pow(float3(193, 243, 118) / 255, 2.2), IN.uv.y)
		, 1.f);
}

