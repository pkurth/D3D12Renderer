#include "sky_rs.hlsli"
#include "light_source.hlsli"


ConstantBuffer<sky_cb> skyIntensity : register(b1);
ConstantBuffer<spherical_harmonics> sh : register(b2);

struct ps_input
{
	float3 uv		: TEXCOORDS;
};

struct ps_output
{
	float4 color			: SV_Target0;
	float2 screenVelocity	: SV_Target1;
	uint objectID			: SV_Target2;
};

[RootSignature(SKY_SH_RS)]
ps_output main(ps_input IN)
{
	ps_output OUT;
	OUT.color = float4(sh.evaluate(normalize(IN.uv)) * skyIntensity.intensity, 0.f);
	OUT.screenVelocity = float2(0.f, 0.f); // TODO: This is of course not the correct screen velocity for the sky.
	OUT.objectID = 0xFFFFFFFF; // -1.
	return OUT;
}
