#include "math.hlsli"
#include "sky_rs.hlsli"


ConstantBuffer<sky_intensity_cb> skyIntensity : register(b1);

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

[RootSignature(SKY_PROCEDURAL_RS)]
ps_output main(ps_input IN)
{
	float3 dir = normalize(IN.uv);
	float2 panoUV = float2(atan2(-dir.x, -dir.z), acos(dir.y)) * invAtan;

	//while (panoUV.x < 0.f) { panoUV.x += 1.f; }
	//while (panoUV.y < 0.f) { panoUV.y += 1.f; }
	//while (panoUV.x > 1.f) { panoUV.x -= 1.f; }
	//while (panoUV.y > 1.f) { panoUV.y -= 1.f; }

	float step = 1.f / 20.f;

	int x = (int)(panoUV.x / step) & 1;
	int y = (int)(panoUV.y / step) & 1;

	float intensity = remap((float)(x == y), 0.f, 1.f, 0.05f, 1.f) * skyIntensity.intensity;

	ps_output OUT;
	OUT.color = float4(intensity * float3(0.4f, 0.6f, 0.2f), 1.f);
	OUT.screenVelocity = float2(0.f, 0.f); // TODO: This is of course not the correct screen velocity for the sky.
	OUT.objectID = 0xFFFFFFFF; // -1.
	return OUT;
}
