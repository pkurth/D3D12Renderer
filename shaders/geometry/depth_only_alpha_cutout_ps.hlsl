#include "depth_only_rs.hlsli"
#include "camera.hlsli"
#include "math.hlsli"

ConstantBuffer<depth_only_object_id_cb> id			: register(b0, space1);
ConstantBuffer<depth_only_camera_jitter_cb> jitter	: register(b1, space1);

Texture2D<float4> alpha								: register(t0, space1);
SamplerState wrapSampler							: register(s0);

struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3 ndc				: NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;
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
	OUT.screenVelocity = screenSpaceVelocity(IN.ndc, IN.prevFrameNDC, jitter.jitter, jitter.prevFrameJitter);
	OUT.objectID = id.id;
	return OUT;
}
