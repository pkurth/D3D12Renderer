#include "terrain_rs.hlsli"
#include "normal.hlsli"

Texture2D<float2> normals	: register(t1);
SamplerState linearSampler	: register(s0);
SamplerState pointSampler	: register(s1);

struct ps_input
{
	float2 uv	: TEXCOORDS;
};

struct ps_output
{
	float4 hdrColor				: SV_Target0;
	float4 worldNormalRoughness	: SV_Target1;
};

[RootSignature(TERRAIN_RS)]
ps_output main(ps_input IN)
{
	float2 n = normals.Sample(linearSampler, IN.uv);
	float3 N = normalize(float3(n.x, 1.f, n.y));
	float NdotL = dot(N, -normalize(float3(-0.6f, -1.f, -0.3f)));

	const float roughness = 0.1f;

	float3 col = saturate(NdotL) * float3(1.f, 0.93f, 0.76f) * 3 + saturate(-NdotL) * float3(0.2f, 0.3f, 0.8f) * 0.1f;
	col *= 0.3f;

	ps_output OUT;
	OUT.hdrColor = float4(col, 1);
	OUT.worldNormalRoughness = float4(packNormal(N), roughness, 0.f);
	return OUT;
}
