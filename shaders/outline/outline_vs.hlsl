#include "outline_rs.hlsli"


ConstantBuffer<outline_cb> outline : register(b0);

struct vs_input
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORDS;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
};

struct vs_output
{
	float4 position			: SV_POSITION;
};

[RootSignature(OUTLINE_MARKER_RS)]
vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(outline.mvp, float4(IN.position, 1.f));
	return OUT;
}
