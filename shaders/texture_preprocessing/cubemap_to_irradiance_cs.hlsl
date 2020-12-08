#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 2), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "DescriptorTable( UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#include "cs.hlsli"

#define BLOCK_SIZE 16

cbuffer cubemap_to_irradiance_cb : register(b0)
{
	uint irradianceMapSize;				// Size of the cubemap face in pixels.
	float uvzScale;
};

TextureCube<float4> srcTexture : register(t0);

RWTexture2DArray<float4> outIrradiance : register(u0);

SamplerState linearRepeatSampler : register(s0);

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


[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	// Cubemap texture coords.
	uint3 texCoord = IN.dispatchThreadID;

	// First check if the thread is in the cubemap dimensions.
	if (texCoord.x >= irradianceMapSize || texCoord.y >= irradianceMapSize) return;

	// Map the UV coords of the cubemap face to a direction.
	// [(0, 0), (1, 1)] => [(-0.5, -0.5), (0.5, 0.5)]
	float3 dir = float3(texCoord.xy / float(irradianceMapSize) - 0.5f, 0.5f);
	dir = normalize(mul(rotateUV[texCoord.z], dir));

	float3 up = float3(0.f, 1.f, 0.f);
	float3 right = cross(up, dir);
	up = cross(dir, right);

	float3 irradiance = float3(0.f, 0.f, 0.f);


	uint srcWidth, srcHeight, numMipLevels;
	srcTexture.GetDimensions(0, srcWidth, srcHeight, numMipLevels);

	float sampleMipLevel = log2((float)srcWidth / (float)irradianceMapSize);

	const float sampleDelta = 0.025f;
	float nrSamples = 0.f;
	for (float phi = 0.f; phi < 2.f * pi; phi += sampleDelta)
	{
		for (float theta = 0.f; theta < 0.5f * pi; theta += sampleDelta)
		{
			float sinTheta, cosTheta;
			float sinPhi, cosPhi;
			sincos(theta, sinTheta, cosTheta);
			sincos(phi, sinPhi, cosPhi);

			// Spherical to cartesian (in tangent space).
			float3 tangentSample = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
			// Tangent space to world.
			float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * dir;

			sampleVec.z *= uvzScale;

			float4 color = srcTexture.SampleLevel(linearRepeatSampler, sampleVec, sampleMipLevel);
			irradiance += color.xyz * cosTheta * sinTheta;
			nrSamples++;
		}
	}

	irradiance = pi * irradiance * (1.f / float(nrSamples));

	outIrradiance[texCoord] = float4(irradiance, 1.f);
}