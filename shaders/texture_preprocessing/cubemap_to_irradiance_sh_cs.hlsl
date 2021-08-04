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


cbuffer cubemap_to_irradiance_sh_cb : register(b0)
{
	uint cubemapSize;
	float uvzScale;
};


TextureCube<float4> srcTexture : register(t0);

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


#define BLOCK_SIZE 32 // 1024 threads

groupshared float4 gs_p0[32];
groupshared float4 gs_p1[32];
groupshared float4 gs_p2[32];
groupshared float4 gs_p3[32];
groupshared float4 gs_p4[32];
groupshared float4 gs_p5[32];
groupshared float4 gs_p6[32];
groupshared float4 gs_p7[32];
groupshared float4 gs_p8[32];
groupshared float gs_totalWeight[32];

static float areaElement(float2 xy)
{
	return atan2(xy.x * xy.y, length(float3(xy, 1.f)));
}

static float texelSolidAngle(float2 xy, float size)
{
	float invRes = 1.f / size;

	float u = ((float)xy.x + 0.5f) * invRes;
	float v = ((float)xy.y + 0.5f) * invRes;

	u = 2.f * u - 1.f;
	v = 2.f * v - 1.f;

	return areaElement(float2(u - invRes, v - invRes)) - areaElement(float2(u - invRes, v + invRes))
		- areaElement(float2(u + invRes, v - invRes)) + areaElement(float2(u + invRes, v + invRes));
}

static float sh0(float3 dir) { return 0.282095f; }
static float sh1(float3 dir) { return 0.488603f * dir.y; }
static float sh2(float3 dir) { return 0.488603f * dir.z; }
static float sh3(float3 dir) { return 0.488603f * dir.x; }
static float sh4(float3 dir) { return 1.092548f * dir.x * dir.y; }
static float sh5(float3 dir) { return 1.092548f * dir.y * dir.z; }
static float sh6(float3 dir) { return 0.315392f * (3.f * dir.z * dir.z - 1.f); }
static float sh7(float3 dir) { return 1.092548f * dir.z * dir.x; }
static float sh8(float3 dir) { return 0.546274f * (dir.x * dir.x - dir.y * dir.y); }


[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	// https://computergraphics.stackexchange.com/questions/4997/spherical-harmonics-diffuse-cubemap-how-to-get-coefficients

	uint3 texCoord = IN.dispatchThreadID;

	float3 dir = float3(texCoord.xy / float(cubemapSize) - 0.5f, 0.5f);
	dir = normalize(mul(rotateUV[texCoord.z], dir));
	dir.z *= uvzScale;


	float4 radiance = srcTexture.SampleLevel(linearRepeatSampler, dir, 0);
	float dw = texelSolidAngle(float2(texCoord.xy), float(cubemapSize));

	float4 weightedRadiance = radiance * dw * (1.f / pi); // Premultiply with 1/pi (diffuse BRDF). TODO: Is this correct?

	// Band 0.
	float4 p0 = weightedRadiance * sh0(dir);

	// Band 1.
	float4 p1 = weightedRadiance * sh1(dir);
	float4 p2 = weightedRadiance * sh2(dir);
	float4 p3 = weightedRadiance * sh3(dir);

	// Band 2.
	float4 p4 = weightedRadiance * sh4(dir);
	float4 p5 = weightedRadiance * sh5(dir);
	float4 p6 = weightedRadiance * sh6(dir);
	float4 p7 = weightedRadiance * sh7(dir);
	float4 p8 = weightedRadiance * sh8(dir);


	// Convolve with cosine lobe to get irradiance.
	p0 *= pi;
	p1 *= (2.f / 3.f) * pi;
	p2 *= (2.f / 3.f) * pi;
	p3 *= (2.f / 3.f) * pi;
	p4 *= (1.f / 4.f) * pi;
	p5 *= (1.f / 4.f) * pi;
	p6 *= (1.f / 4.f) * pi;
	p7 *= (1.f / 4.f) * pi;
	p8 *= (1.f / 4.f) * pi;







	//float4 waveP0 = WaveActiveSum(p0);
	//float4 waveP1 = WaveActiveSum(p1);
	//float4 waveP2 = WaveActiveSum(p2);
	//float4 waveP3 = WaveActiveSum(p3);
	//float4 waveP4 = WaveActiveSum(p4);
	//float4 waveP5 = WaveActiveSum(p5);
	//float4 waveP6 = WaveActiveSum(p6);
	//float4 waveP7 = WaveActiveSum(p7);
	//float4 waveP8 = WaveActiveSum(p8);
	//float waveWeight = WaveActiveSum(weight);

	//if (WaveIsFirstLane())
	//{
	//	uint writeIndex = IN.groupIndex / 32;
	//	gs_p0[writeIndex]			= waveP0;
	//	gs_p1[writeIndex]			= waveP1;
	//	gs_p2[writeIndex]			= waveP2;
	//	gs_p3[writeIndex]			= waveP3;
	//	gs_p4[writeIndex]			= waveP4;
	//	gs_p5[writeIndex]			= waveP5;
	//	gs_p6[writeIndex]			= waveP6;
	//	gs_p7[writeIndex]			= waveP7;
	//	gs_p8[writeIndex]			= waveP8;
	//	gs_totalWeight[writeIndex]	= waveWeight;
	//}

	//GroupMemoryBarrierWithGroupSync();

	//
	//if (IN.groupIndex < 32) // First wave.
	//{
	//	float4 p0Total				= WaveActiveSum(gs_p0[IN.groupIndex]);
	//	float4 p1Total				= WaveActiveSum(gs_p1[IN.groupIndex]);
	//	float4 p2Total				= WaveActiveSum(gs_p2[IN.groupIndex]);
	//	float4 p3Total				= WaveActiveSum(gs_p3[IN.groupIndex]);
	//	float4 p4Total				= WaveActiveSum(gs_p4[IN.groupIndex]);
	//	float4 p5Total				= WaveActiveSum(gs_p5[IN.groupIndex]);
	//	float4 p6Total				= WaveActiveSum(gs_p6[IN.groupIndex]);
	//	float4 p7Total				= WaveActiveSum(gs_p7[IN.groupIndex]);
	//	float4 p8Total				= WaveActiveSum(gs_p8[IN.groupIndex]);
	//	float weightTotal			= WaveActiveSum(gs_totalWeight[IN.groupIndex]);
	//}
}