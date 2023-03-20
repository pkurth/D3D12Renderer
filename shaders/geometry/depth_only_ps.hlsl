#include "depth_only_rs.hlsli"
#include "camera.hlsli"
#include "math.hlsli"

ConstantBuffer<camera_cb> camera	: register(b0, space1);

struct ps_input
{
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
	ps_output OUT;
	OUT.screenVelocity = screenSpaceVelocity(IN.ndc, IN.prevFrameNDC, camera.jitter, camera.prevFrameJitter);
	OUT.objectID = IN.objectID;
	return OUT;
}
