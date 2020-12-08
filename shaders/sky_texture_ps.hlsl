#include "sky_rs.hlsli"


ConstantBuffer<sky_intensity_cb> skyIntensity : register(b1);

struct ps_input
{
	float3 uv		: TEXCOORDS;
};

SamplerState texSampler	: register(s0);
TextureCube<float4> tex	: register(t0);

[RootSignature(SKY_TEXTURE_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	return tex.Sample(texSampler, IN.uv) * skyIntensity.intensity;
}
