#include "model_rs.hlsl"


ConstantBuffer<transform_cb> transform : register(b0);

struct vs_input
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORDS;
	float3 normal		: NORMAL;
};

struct vs_output
{
	float2 uv		: TEXCOORD;
	float3 normal	: NORMAL;
	float4 position : SV_POSITION;
};

[RootSignature(MODEL_RS)]
vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	OUT.normal = mul(transform.m, float4(IN.normal, 0.f)).xyz;
	OUT.uv = IN.uv;
	return OUT;
}
