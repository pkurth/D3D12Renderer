#include "depth_only_rs.hlsli"

ConstantBuffer<depth_only_object_id_cb> id : register(b1);

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
	float2 screenUV = (IN.ndc.xy / IN.ndc.z) * 0.5f + 0.5f;
	float2 prevFrameScreenUV = (IN.prevFrameNDC.xy / IN.prevFrameNDC.z) * 0.5f + 0.5f;

	ps_output OUT;
	OUT.screenVelocity = screenUV - prevFrameScreenUV;
	OUT.objectID = id.id;
	return OUT;
}
