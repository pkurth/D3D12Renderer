#include "model_rs.hlsl"

struct ps_input
{
	float2 uv		: TEXCOORD;
	float3 normal	: NORMAL;
};

SamplerState texSampler	: register(s0);
Texture2D<float4> tex	: register(t0);

[RootSignature(MODEL_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float4 albedo = tex.Sample(texSampler, IN.uv) * 10.f;

	static const float3 L = normalize(float3(1.f, 0.8f, 0.3f));
	return clamp(dot(L, normalize(IN.normal)), 0.1f, 1.f) * albedo;
}
