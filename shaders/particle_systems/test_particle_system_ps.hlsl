#include "particles_rs.hlsli"

struct ps_input
{
	float relLife		: RELLIFE;
	float2 uv			: TEXCOORDS;
};

Texture2D<float4> tex			: register(t0, space1);
SamplerState texSampler			: register(s0, space1);


[RootSignature(PARTICLES_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float4 color = tex.Sample(texSampler, IN.uv);
	return color * color.a * smoothstep(IN.relLife, 0.f, 0.1f) * 1.f;
}
