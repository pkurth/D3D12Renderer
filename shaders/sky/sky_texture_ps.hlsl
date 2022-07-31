#include "sky_rs.hlsli"
#include "math.hlsli"

ConstantBuffer<sky_cb> cb : register(b1);

struct ps_input
{
	float3 uv				: TEXCOORDS;
	float3 ndc				: NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;
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
	OUT.color = float4((tex.Sample(texSampler, IN.uv) * cb.intensity).xyz, 0.f);
	OUT.screenVelocity = screenSpaceVelocity(IN.ndc, IN.prevFrameNDC, cb.jitter, cb.prevFrameJitter);
	OUT.objectID = 0xFFFFFFFF; // -1.
	return OUT;
}
