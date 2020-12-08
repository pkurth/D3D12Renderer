// Adapted from Source: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli

#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 6), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "DescriptorTable( UAV(u0, numDescriptors = 4, flags = DESCRIPTORS_VOLATILE) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#include "cs.hlsli"

#define BLOCK_SIZE 8 // In one dimension. 64 in total.

#define WIDTH_HEIGHT_EVEN		0 // Both the width and the height of the texture are even.
#define WIDTH_ODD_HEIGHT_EVEN	1 // The texture width is odd and the height is even.
#define WIDTH_EVEN_HEIGHT_ODD	2 // The texture width is even and the height is odd.
#define WIDTH_HEIGHT_ODD		3 // Both the width and height of the texture are odd.

cbuffer generate_mips_cb : register(b0)
{
	uint srcMipLevel;				// Texture level of source mip
	uint numMipLevelsToGenerate;	// The shader can generate up to 4 mips at once.
	uint srcDimensionFlags;			// Flags specifying whether width and height are even or odd (see above).
	bool isSRGB;					// Must apply gamma correction to sRGB textures.
	float2 texelSize;				// 1.0 / OutMip1.Dimensions
};

// Source mip map.
Texture2D<float4> srcMip		: register(t0);

// Write up to 4 mip map levels. These are successive levels after the source.
RWTexture2D<float4> outMip1		: register(u0);
RWTexture2D<float4> outMip2		: register(u1);
RWTexture2D<float4> outMip3		: register(u2);
RWTexture2D<float4> outMip4		: register(u3);

// Linear clamp sampler.
SamplerState linearClampSampler : register(s0);

// The reason for separating channels is to reduce bank conflicts in the
// local data memory controller.  A large stride will cause more threads
// to collide on the same memory bank.
groupshared float gs_R[BLOCK_SIZE * BLOCK_SIZE];
groupshared float gs_G[BLOCK_SIZE * BLOCK_SIZE];
groupshared float gs_B[BLOCK_SIZE * BLOCK_SIZE];
groupshared float gs_A[BLOCK_SIZE * BLOCK_SIZE];

void storeColorToSharedMemory(uint index, float4 color)
{
	gs_R[index] = color.r;
	gs_G[index] = color.g;
	gs_B[index] = color.b;
	gs_A[index] = color.a;
}

float4 loadColorFromSharedMemory(uint index)
{
	return float4(gs_R[index], gs_G[index], gs_B[index], gs_A[index]);
}

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
	float4 src1 = (float4)0;

	switch (srcDimensionFlags)
	{
	case WIDTH_HEIGHT_EVEN:
	{
		float2 uv = texelSize * (IN.dispatchThreadID.xy + 0.5f);
		src1 = srcMip.SampleLevel(linearClampSampler, uv, srcMipLevel);
	} break;

	case WIDTH_ODD_HEIGHT_EVEN:
	{
		// > 2:1 in X dimension
		// Use 2 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
		// horizontally.
		float2 uv1 = texelSize * (IN.dispatchThreadID.xy + float2(0.25f, 0.5f));
		float2 off = texelSize * float2(0.5f, 0.f);

		src1 = 0.5f * (srcMip.SampleLevel(linearClampSampler, uv1, srcMipLevel) +
			srcMip.SampleLevel(linearClampSampler, uv1 + off, srcMipLevel));
	} break;

	case WIDTH_EVEN_HEIGHT_ODD:
	{
		// > 2:1 in Y dimension
		// Use 2 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
		// vertically.
		float2 uv1 = texelSize * (IN.dispatchThreadID.xy + float2(0.5f, 0.25f));
		float2 off = texelSize * float2(0.f, 0.5f);

		src1 = 0.5f * (srcMip.SampleLevel(linearClampSampler, uv1, srcMipLevel) +
			srcMip.SampleLevel(linearClampSampler, uv1 + off, srcMipLevel));
	} break;

	case WIDTH_HEIGHT_ODD:
	{
		// > 2:1 in in both dimensions
		// Use 4 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
		// in both directions.
		float2 uv1 = texelSize * (IN.dispatchThreadID.xy + float2(0.25f, 0.25f));
		float2 off = texelSize * 0.5f;

		src1 = srcMip.SampleLevel(linearClampSampler, uv1, srcMipLevel);
		src1 += srcMip.SampleLevel(linearClampSampler, uv1 + float2(off.x, 0.0), srcMipLevel);
		src1 += srcMip.SampleLevel(linearClampSampler, uv1 + float2(0.0, off.y), srcMipLevel);
		src1 += srcMip.SampleLevel(linearClampSampler, uv1 + float2(off.x, off.y), srcMipLevel);
		src1 *= 0.25;
	} break;
	}

	outMip1[IN.dispatchThreadID.xy] = packColor(src1);

	if (numMipLevelsToGenerate == 1)
	{
		return;
	}

	storeColorToSharedMemory(IN.groupIndex, src1);

	GroupMemoryBarrierWithGroupSync();

	if ((IN.groupIndex & 0x9) == 0)
	{
		float4 src2 = loadColorFromSharedMemory(IN.groupIndex + 0x01);
		float4 src3 = loadColorFromSharedMemory(IN.groupIndex + 0x08);
		float4 src4 = loadColorFromSharedMemory(IN.groupIndex + 0x09);
		src1 = 0.25f * (src1 + src2 + src3 + src4);

		outMip2[IN.dispatchThreadID.xy / 2] = packColor(src1);
		storeColorToSharedMemory(IN.groupIndex, src1);
	}

	if (numMipLevelsToGenerate == 2)
	{
		return;
	}

	GroupMemoryBarrierWithGroupSync();

	if ((IN.groupIndex & 0x1B) == 0)
	{
		float4 src2 = loadColorFromSharedMemory(IN.groupIndex + 0x02);
		float4 src3 = loadColorFromSharedMemory(IN.groupIndex + 0x10);
		float4 src4 = loadColorFromSharedMemory(IN.groupIndex + 0x12);
		src1 = 0.25f * (src1 + src2 + src3 + src4);

		outMip3[IN.dispatchThreadID.xy / 4] = packColor(src1);
		storeColorToSharedMemory(IN.groupIndex, src1);
	}

	if (numMipLevelsToGenerate == 3)
	{
		return;
	}

	GroupMemoryBarrierWithGroupSync();

	if (IN.groupIndex == 0)
	{
		float4 src2 = loadColorFromSharedMemory(IN.groupIndex + 0x04);
		float4 src3 = loadColorFromSharedMemory(IN.groupIndex + 0x20);
		float4 src4 = loadColorFromSharedMemory(IN.groupIndex + 0x24);
		src1 = 0.25f * (src1 + src2 + src3 + src4);

		outMip4[IN.dispatchThreadID.xy / 8] = packColor(src1);
	}
}


