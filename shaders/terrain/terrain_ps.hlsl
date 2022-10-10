#include "terrain_rs.hlsli"

struct ps_input
{
	float2 uv	: TEXCOORDS;
};

[RootSignature(TERRAIN_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return float4(1, 0, 0, 1);
}
