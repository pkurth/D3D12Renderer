#include "visualization_rs.hlsli"

ConstantBuffer<visualization_textured_cb> cb	: register(b1);

Texture2D<float4> tex							: register(t0);
SamplerState texSampler							: register(s0);

struct ps_input
{
	float2 uv				: TEXCOORDS;
};

[RootSignature(FLAT_UNLIT_TEXTURED_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float2 uv = lerp(cb.uv0, cb.uv1, IN.uv);
	return cb.color * tex.Sample(texSampler, uv);
}
