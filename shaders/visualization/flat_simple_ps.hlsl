#include "flat_simple_rs.hlsli"

ConstantBuffer<flat_simple_color_cb> color : register(b1);

struct ps_input
{
	float3 viewPosition	: VIEW_POSITION;
	float3 normal		: NORMAL;
};

[RootSignature(FLAT_SIMPLE_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float ndotv = saturate(-dot(normalize(IN.viewPosition), normalize(IN.normal))) * 0.8 + 0.2;
	return ndotv * color.color;
}
