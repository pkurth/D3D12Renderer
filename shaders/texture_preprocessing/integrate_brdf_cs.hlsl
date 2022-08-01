#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 1), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )"

#include "cs.hlsli"
#include "brdf.hlsli"
#include "random.hlsli"

#define BLOCK_SIZE 16

cbuffer cb : register(b0)
{
	uint textureDim;
};

RWTexture2D<float2> outBRDF : register(u0);

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	if (IN.dispatchThreadID.x >= textureDim || IN.dispatchThreadID.y >= textureDim)
	{
		return;
	}

	float NdotV = float(IN.dispatchThreadID.x) / (textureDim - 1);
	float roughness = float(IN.dispatchThreadID.y) / (textureDim - 1);


	float3 V;
	V.x = sqrt(1.f - NdotV * NdotV);
	V.y = 0.f;
	V.z = NdotV;

	float A = 0.f;
	float B = 0.f;

	float3 N = float3(0.f, 0.f, 1.f);

	const uint SAMPLE_COUNT = 1024u;
	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = hammersley(i, SAMPLE_COUNT);
		float3 H = importanceSampleGGX(Xi, N, roughness).xyz;
		float3 L = normalize(2.f * dot(V, H) * H - V);

		float NdotL = max(L.z, 0.f);
		float NdotH = max(H.z, 0.f);
		float VdotH = max(dot(V, H), 0.f);

		if (NdotL > 0.f)
		{
			float G = geometrySmith(NdotL, NdotV, roughness);
			float G_Vis = (G * VdotH) / (NdotH * NdotV);
			float Fc = pow(1.f - VdotH, 5.f);

			A += (1.f - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}
	A /= float(SAMPLE_COUNT);
	B /= float(SAMPLE_COUNT);

	float2 integratedBRDF = float2(A, B);
	outBRDF[IN.dispatchThreadID.xy] = integratedBRDF;
}