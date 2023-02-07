#include "grass_rs.hlsli"

struct ps_input
{
	float2 uv		: TEXCOORDS;
	float3 normal	: NORMAL;

	float4 screenPosition	: SV_POSITION;
	bool isFrontFace		: SV_IsFrontFace;
};

[RootSignature(GRASS_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float3 L = -normalize(vec3(-0.6f, -1.f, -0.3f));
	float3 N = normalize(IN.normal);
	if (!IN.isFrontFace)
	{
		N = -N;
	}

	float NdotL = 1.f;// saturate(dot(N, L));

	return float4(
		NdotL * lerp(pow(float3(121, 208, 33) / 255, 2.2), pow(float3(193, 243, 118) / 255, 2.2), IN.uv.y)
		, 1.f);
}

