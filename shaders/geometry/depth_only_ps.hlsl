#include "depth_only_rs.hlsli"
#include "camera.hlsli"
#include "math.hlsli"

ConstantBuffer<depth_only_object_id_cb> id			: register(b0, space1);
ConstantBuffer<depth_only_camera_jitter_cb> jitter	: register(b1, space1);

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
	ps_output OUT;
	OUT.screenVelocity = screenSpaceVelocity(IN.ndc, IN.prevFrameNDC, jitter.jitter, jitter.prevFrameJitter);
	OUT.objectID = id.id;
	return OUT;
}
