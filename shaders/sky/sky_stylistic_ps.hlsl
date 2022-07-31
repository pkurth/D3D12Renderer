#include "math.hlsli"
#include "sky_rs.hlsli"
#include "stylistic_sky.hlsli"


ConstantBuffer<sky_cb> cb : register(b1);

struct ps_input
{
	float3 uv				: TEXCOORDS;
	float3 ndc				: NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;
};

struct ps_output
{
	float4 color			: SV_Target0;
	float2 screenVelocity	: SV_Target1;
	uint objectID			: SV_Target2;
};

[RootSignature(SKY_STYLISTIC_RS)]
ps_output main(ps_input IN)
{
	float3 V = normalize(IN.uv);
	float3 L = -cb.sunDirection;

	float3 color = stylisticSky(V, L);

	color *= cb.intensity;

	ps_output OUT;
	OUT.color = float4(max(color, 0.f), 0.f);
	OUT.screenVelocity = screenSpaceVelocity(IN.ndc, IN.prevFrameNDC, cb.jitter, cb.prevFrameJitter);
	OUT.objectID = 0xFFFFFFFF; // -1.
	return OUT;
}
