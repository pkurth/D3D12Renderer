#include "depth_only_rs.hlsli"
#include "camera.hlsli"

ConstantBuffer<depth_only_object_id_cb> id			: register(b1);
ConstantBuffer<depth_only_camera_jitter_cb> jitter	: register(b2);

struct ps_input
{
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
	float2 ndc = (IN.ndc.xy / IN.ndc.z) - jitter.jitter;
	float2 prevNDC = (IN.prevFrameNDC.xy / IN.prevFrameNDC.z) - jitter.prevFrameJitter;

	float2 motion = (prevNDC - ndc) * float2(0.5f, -0.5f);	

	ps_output OUT;
	OUT.screenVelocity = motion;
	OUT.objectID = id.id;
	return OUT;
}
