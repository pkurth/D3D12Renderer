#include "model_rs.hlsl"

struct ps_input
{
	float2 uv		: TEXCOORD;
	float3 normal	: NORMAL;
};

[RootSignature(MODEL_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	static const float3 L = normalize(float3(1.f, 0.8f, 0.3f));
	return saturate(dot(L, normalize(IN.normal))) * float4(1.f, 1.f, 1.f, 1.f);
}
