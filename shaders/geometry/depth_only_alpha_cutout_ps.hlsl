#include "depth_only_rs.hlsli"
#include "camera.hlsli"
#include "math.hlsli"

ConstantBuffer<camera_cb> camera	: register(b0, space1);

Texture2D<float4> alpha				: register(t0, space1);
SamplerState wrapSampler			: register(s0);

struct ps_input
{
	float2 uv						: TEXCOORDS;
	float3 ndc						: NDC;
	float3 prevFrameNDC				: PREV_FRAME_NDC;

	nointerpolation uint objectID	: OBJECT_ID;
};

struct ps_output
{
	float2 screenVelocity	: SV_Target0;
	uint objectID			: SV_Target1;
};

ps_output main(ps_input IN)
{
	float a = alpha.Sample(wrapSampler, IN.uv).w;
	clip(a - 0.5f);

	ps_output OUT;
	OUT.screenVelocity = screenSpaceVelocity(IN.ndc, IN.prevFrameNDC, camera.jitter, camera.prevFrameJitter);
	OUT.objectID = IN.objectID;
	return OUT;
}
