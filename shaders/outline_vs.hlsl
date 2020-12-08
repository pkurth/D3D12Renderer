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
	float4 color			: COLOR;
	float4 position			: SV_POSITION;
};

[RootSignature(OUTLINE_RS)]
vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(outline.mvp, float4(IN.position + IN.normal * 1.2f, 1.f));
	OUT.color = outline.color;
	return OUT;
}
