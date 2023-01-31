#include "proc_placement_rs.hlsli"
#include "camera.hlsli"

ConstantBuffer<camera_cb> camera						: register(b0);
StructuredBuffer<placement_point> placementPoints		: register(t0);

struct vs_input
{
	float3 position		: POSITION;
	uint instanceID		: SV_InstanceID;
};

struct vs_output
{
	float4 position		: SV_POSITION;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(camera.viewProj, float4(IN.position * 0.2f + placementPoints[IN.instanceID].position, 1.f));
	return OUT;
}
