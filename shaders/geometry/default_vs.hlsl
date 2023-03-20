#include "default_pbr_rs.hlsli"
#include "camera.hlsli"

StructuredBuffer<float4x4> transforms	: register(t0);
ConstantBuffer<camera_cb> camera		: register(b1, space1);

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
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;

	float4 position			: SV_POSITION;
};

vs_output main(vs_input IN)
{
	float4x4 m = transforms[IN.instanceID];
	float4 worldPosition = mul(m, float4(IN.position, 1.f));

	vs_output OUT;
	OUT.position = mul(camera.viewProj, worldPosition);

	OUT.uv = IN.uv;
	OUT.worldPosition = worldPosition.xyz;

	float3 normal = normalize(mul(m, float4(IN.normal, 0.f)).xyz);
	float3 tangent = normalize(mul(m, float4(IN.tangent, 0.f)).xyz);
	float3 bitangent = normalize(cross(normal, tangent));
	OUT.tbn = float3x3(tangent, bitangent, normal);

	return OUT;
}
