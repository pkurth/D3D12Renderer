#include "particles_rs.hlsli"

struct ps_input
{
	float4 color			: COLOR;
};

[RootSignature(PARTICLES_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return IN.color;
}
