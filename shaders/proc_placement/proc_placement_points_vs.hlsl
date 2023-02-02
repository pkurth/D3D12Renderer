#include "proc_placement_rs.hlsli"
#include "camera.hlsli"

ConstantBuffer<camera_cb> camera						: register(b0);
StructuredBuffer<placement_transform> transforms		: register(t0);

struct vs_input
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORDS;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;

	uint instanceID		: SV_InstanceID;
};

struct vs_output
{
	float2 uv			: TEXCOORDS;
	float4 position		: SV_POSITION;
};

vs_output main(vs_input IN)
{
	float4x4 mvp = mul(camera.viewProj, transforms[IN.instanceID].m);

	vs_output OUT;
	OUT.position = mul(mvp, float4(IN.position * 0.02f, 1.f));
	OUT.uv = IN.uv;
	return OUT;
}
