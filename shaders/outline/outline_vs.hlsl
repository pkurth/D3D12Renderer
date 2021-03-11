#include "outline_rs.hlsli"


ConstantBuffer<outline_marker_cb> outline : register(b0);

struct vs_input
{
	float3 position		: POSITION;
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
