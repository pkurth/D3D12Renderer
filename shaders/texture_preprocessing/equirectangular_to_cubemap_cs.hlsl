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

#include "cs.hlsli"

#define BLOCK_SIZE 16

cbuffer equirectangular_to_cubemap_cb : register(b0)
{
	uint cubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	uint firstMip;					// The first mip level to generate.
	uint numMipLevelsToGenerate;	// The number of mips to generate.
	bool isSRGB;					// Must apply gamma correction to sRGB textures.
};

// Source texture as an equirectangular panoramic image.
// It is assumed that the src texture has a full mipmap chain.
Texture2D<float4> srcTexture : register(t0);

// Destination texture as a mip slice in the cubemap texture (texture array with 6 elements).
RWTexture2DArray<float4> outMip1 : register(u0);
RWTexture2DArray<float4> outMip2 : register(u1);
RWTexture2DArray<float4> outMip3 : register(u2);
RWTexture2DArray<float4> outMip4 : register(u3);
RWTexture2DArray<float4> outMip5 : register(u4);

// Linear repeat sampler.
SamplerState linearRepeatSampler : register(s0);


// 1 / PI
static const float invPI = 0.31830988618379067153776752674503f;
static const float inv2PI = 0.15915494309189533576888376337251f;
static const float2 invAtan = float2(inv2PI, invPI);

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

float3 sRGBToLinear(float3 x)
{
	return x < 0.04045f ? x / 12.92f : pow((x + 0.055f) / 1.055f, 2.4f);
}

float3 linearToSRGB(float3 x)
{
	return x < 0.0031308f ? 12.92f * x : 1.055f * pow(abs(x), 1.f / 2.4f) - 0.055f;
}

float4 packColor(float4 x)
{
	if (isSRGB)
	{
		return float4(linearToSRGB(x.rgb), x.a);
	}
	else
	{
		return x;
	}
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
	float3 dir = float3(texCoord.xy / float(cubemapSize) - 0.5f, 0.5f);

	// Rotate to cubemap face
	dir = normalize(mul(rotateUV[texCoord.z], dir));
	dir.z *= -1.f;


	// Convert the world space direction into U,V texture coordinates in the panoramic texture.
	// Source: http://gl.ict.usc.edu/Data/HighResProbes/
	float2 panoUV = float2(atan2(-dir.x, -dir.z), acos(dir.y)) * invAtan;

	outMip1[texCoord] = packColor(srcTexture.SampleLevel(linearRepeatSampler, panoUV, firstMip));

	// Only perform on threads that are a multiple of 2.
	if (numMipLevelsToGenerate > 1 && (IN.groupIndex & 0x11) == 0)
	{
		outMip2[uint3(texCoord.xy / 2, texCoord.z)] = packColor(srcTexture.SampleLevel(linearRepeatSampler, panoUV, firstMip + 1));
	}

	// Only perform on threads that are a multiple of 4.
	if (numMipLevelsToGenerate > 2 && (IN.groupIndex & 0x33) == 0)
	{
		outMip3[uint3(texCoord.xy / 4, texCoord.z)] = packColor(srcTexture.SampleLevel(linearRepeatSampler, panoUV, firstMip + 2));
	}

	// Only perform on threads that are a multiple of 8.
	if (numMipLevelsToGenerate > 3 && (IN.groupIndex & 0x77) == 0)
	{
		outMip4[uint3(texCoord.xy / 8, texCoord.z)] = packColor(srcTexture.SampleLevel(linearRepeatSampler, panoUV, firstMip + 3));
	}

	// Only perform on threads that are a multiple of 16.
	// This should only be thread 0 in this group.
	if (numMipLevelsToGenerate > 4 && (IN.groupIndex & 0xFF) == 0)
	{
		outMip5[uint3(texCoord.xy / 16, texCoord.z)] = packColor(srcTexture.SampleLevel(linearRepeatSampler, panoUV, firstMip + 4));
	}
}
