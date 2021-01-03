#include "default_pbr_rs.hlsli"


ConstantBuffer<transform_cb> transform	: register(b0);

struct vs_input
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORDS;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
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
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));

	OUT.uv = IN.uv;
	OUT.worldPosition = (mul(transform.m, float4(IN.position, 1.f))).xyz;

	float3 normal = normalize(mul(transform.m, float4(IN.normal, 0.f)).xyz);
	float3 tangent = normalize(mul(transform.m, float4(IN.tangent, 0.f)).xyz);
	float3 bitangent = normalize(cross(normal, tangent));
	OUT.tbn = float3x3(tangent, bitangent, normal);

	return OUT;
}
