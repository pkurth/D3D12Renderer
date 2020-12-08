#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 4), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "DescriptorTable( UAV(u0, numDescriptors = 5, flags = DESCRIPTORS_VOLATILE) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#define BLOCK_SIZE 16

#include "brdf.hlsli"
#include "cs.hlsli"


cbuffer prefilter_environment_cb : register(b0)
{
	uint cubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	uint firstMip;					// The first mip level to generate.
	uint numMipLevelsToGenerate;	// The number of mips to generate.
	uint totalNumMipLevels;
};

TextureCube<float4> srcTexture : register(t0);

RWTexture2DArray<float4> outMip1 : register(u0);
RWTexture2DArray<float4> outMip2 : register(u1);
RWTexture2DArray<float4> outMip3 : register(u2);
RWTexture2DArray<float4> outMip4 : register(u3);
RWTexture2DArray<float4> outMip5 : register(u4);

SamplerState linearRepeatSampler : register(s0);


// Transform from dispatch ID to cubemap face direction
static const float3x3 rotateUV[6] = {
	// +X
	float3x3(0,  0,  1,
			 0, -1,  0,
			 -1,  0,  0),
	// -X
    float3x3(0,  0, -1,
    		 0, -1,  0,
    		 1,  0,  0),
	// +Y
	float3x3(1,  0,  0,
	         0,  0,  1,
			 0,  1,  0),
	// -Y
	float3x3(1,  0,  0,
	    	 0,  0, -1,
			 0, -1,  0),
	// +Z
	float3x3(1,  0,  0,
			 0, -1,  0,
			 0,  0,  1),
	// -Z
	float3x3(-1,  0,  0,
		     0,  -1,  0,
			 0,   0, -1)
};


static float4 filter(uint mip, float3 N, float3 V)
{
	float roughness = float(mip) / (totalNumMipLevels - 1);

	const uint SAMPLE_COUNT = 1024u;
	float totalWeight = 0.f;
	float3 prefilteredColor = float3(0.f, 0.f, 0.f);


	uint width, height, numMipLevels;
	srcTexture.GetDimensions(0, width, height, numMipLevels);

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = hammersley(i, SAMPLE_COUNT);
		float3 H = importanceSampleGGX(Xi, N, roughness);
		float3 L = normalize(2.f * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.f);
		float NdotH = max(dot(N, H), 0.f);
		float HdotV = max(dot(H, V), 0.f);
		if (NdotL > 0.f)
		{
			float D = distributionGGX(N, H, roughness);
			float pdf = (D * NdotH / (4.f * HdotV)) + 0.0001f;

			uint resolution = width; // We expect quadratic faces, so width == height.
			float saTexel = 4.f * pi / (6.f * width * height);
			float saSample = 1.f / (SAMPLE_COUNT * pdf + 0.00001f);

			float sampleMipLevel = (roughness == 0.f) ? 0.f : 0.5f * log2(saSample / saTexel);

			prefilteredColor += srcTexture.SampleLevel(linearRepeatSampler, L, sampleMipLevel).xyz * NdotL;
			totalWeight += NdotL;
		}
	}
	prefilteredColor = prefilteredColor / totalWeight;
	return float4(prefilteredColor, 1.f);
}

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	// Cubemap texture coords.
	uint3 texCoord = IN.dispatchThreadID;

	// First check if the thread is in the cubemap dimensions.
	if (texCoord.x >= cubemapSize || texCoord.y >= cubemapSize) return;

	// Map the UV coords of the cubemap face to a direction
	// [(0, 0), (1, 1)] => [(-0.5, -0.5), (0.5, 0.5)]
	float3 N = float3(texCoord.xy / float(cubemapSize) - 0.5f, 0.5f);
	N = normalize(mul(rotateUV[texCoord.z], N));

	float3 R = N;
	float3 V = R;

	outMip1[texCoord] = filter(firstMip, N, V);

	if (numMipLevelsToGenerate > 1 && (IN.groupIndex & 0x11) == 0)
	{
		outMip2[uint3(texCoord.xy / 2, texCoord.z)] = filter(firstMip + 1, N, V);
	}

	if (numMipLevelsToGenerate > 2 && (IN.groupIndex & 0x33) == 0)
	{
		outMip3[uint3(texCoord.xy / 4, texCoord.z)] = filter(firstMip + 2, N, V);
	}

	if (numMipLevelsToGenerate > 3 && (IN.groupIndex & 0x77) == 0)
	{
		outMip4[uint3(texCoord.xy / 8, texCoord.z)] = filter(firstMip + 3, N, V);
	}

	if (numMipLevelsToGenerate > 4 && (IN.groupIndex & 0xFF) == 0)
	{
		outMip5[uint3(texCoord.xy / 16, texCoord.z)] = filter(firstMip + 4, N, V);
	}
}
