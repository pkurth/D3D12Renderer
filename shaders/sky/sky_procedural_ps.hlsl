#include "math.hlsli"
#include "sky_rs.hlsli"


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

[RootSignature(SKY_PROCEDURAL_RS)]
ps_output main(ps_input IN)
{
	float3 dir = normalize(IN.uv);
	float2 panoUV = float2(atan2(-dir.x, -dir.z), acos(dir.y)) * M_INV_ATAN;

	float step = 1.f / 20.f;

	int x = (int)(panoUV.x / step) & 1;
	int y = (int)(panoUV.y / step) & 1;

	float intensity = remap((float)(x == y), 0.f, 1.f, 0.05f, 1.f) * cb.intensity;

	float2 ndc = (IN.ndc.xy / IN.ndc.z) - cb.jitter;
	float2 prevNDC = (IN.prevFrameNDC.xy / IN.prevFrameNDC.z) - cb.prevFrameJitter;

	float2 motion = (prevNDC - ndc) * float2(0.5f, -0.5f);


	ps_output OUT;
	OUT.color = float4(intensity * float3(0.4f, 0.6f, 0.2f), 0.f);
	OUT.screenVelocity = motion;
	OUT.objectID = 0xFFFFFFFF; // -1.
	return OUT;
}
