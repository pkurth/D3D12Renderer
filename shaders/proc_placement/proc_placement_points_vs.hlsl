#include "proc_placement_rs.hlsli"
#include "camera.hlsli"

ConstantBuffer<camera_cb> camera						: register(b0);
StructuredBuffer<placement_transform> transforms		: register(t0);

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
	float4x4 mvp = mul(camera.viewProj, transforms[IN.instanceID].m);

	vs_output OUT;
	OUT.position = mul(mvp, float4(IN.position * 0.2f, 1.f));
	return OUT;
}
