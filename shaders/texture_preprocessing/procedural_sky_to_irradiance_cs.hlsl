#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 4), " \
    "DescriptorTable( UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )"

#include "cs.hlsli"
#include "procedural_sky.hlsli"

#define BLOCK_SIZE 16

cbuffer cb : register(b0)
{
	float3 L;
	uint irradianceMapSize;
};

RWTexture2DArray<float4> outIrradiance : register(u0);

static const float pi = 3.141592653589793238462643383279f;

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


#define NUM_SAMPLES 1024

groupshared float3 radianceSamples[NUM_SAMPLES];
groupshared float3 directions[NUM_SAMPLES];

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	for (uint i = IN.groupIndex; i < NUM_SAMPLES; i += BLOCK_SIZE * BLOCK_SIZE)
	{
		float3 V = sphericalFibonacci(i, NUM_SAMPLES);
		radianceSamples[i] = proceduralSkySimple(V, L);
		directions[i] = V;
	}

	GroupMemoryBarrierWithGroupSync();


	uint3 texCoord = IN.dispatchThreadID;
	if (texCoord.x >= irradianceMapSize || texCoord.y >= irradianceMapSize) return;


	float3 V = float3(texCoord.xy / float(irradianceMapSize) - 0.5f, 0.5f);
	V = normalize(mul(rotateUV[texCoord.z], V));

	float3 result = 0.f;
	float totalWeight = 0.f;

	for (uint j = 0; j < NUM_SAMPLES; ++j)
	{
		float3 rad = radianceSamples[j];
		float3 L = directions[j];

		float weight = max(0.f, dot(V, L));
		result += rad * weight;
		totalWeight += weight;
	}

	result *= 1.f / max(totalWeight, 1e-4f);

	outIrradiance[texCoord] = float4(result, 1.f);
}
