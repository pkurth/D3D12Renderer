#include "sky_rs.hlsli"


ConstantBuffer<sky_intensity_cb> skyIntensity : register(b1);

struct ps_input
{
	float3 uv		: TEXCOORDS;
};

SamplerState texSampler	: register(s0);
TextureCube<float4> tex	: register(t0);

struct ps_output
{
	float4 color			: SV_Target0;
	float2 screenVelocity	: SV_Target1;
	uint objectID			: SV_Target2;
};

[RootSignature(SKY_TEXTURE_RS)]
ps_output main(ps_input IN)
{
	ps_output OUT;
	OUT.color = float4((tex.Sample(texSampler, IN.uv) * skyIntensity.intensity).xyz, 1.f);
	OUT.screenVelocity = float2(0.f, 0.f); // TODO: This is of course not the correct screen velocity for the sky.
	OUT.objectID = 0xFFFFFFFF; // -1.
	return OUT;
}
