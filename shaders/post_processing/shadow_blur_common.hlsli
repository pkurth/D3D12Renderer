
ConstantBuffer<shadow_blur_cb> cb		: register(b0);

Texture2D<float> input		            : register(t0); // Usually half res in horizontal pass, full res in vertical pass.
Texture2D<float> depthBuffer            : register(t1); // Full res.

RWTexture2D<float> output	            : register(u0); // Full res.

SamplerState pointSampler				: register(s0);
SamplerState linearSampler				: register(s1);

#define KERNEL_RADIUS 8.f

// https://developer.nvidia.com/sites/default/files/akamai/gamedev/files/gdc12/GDC12_Bavoil_Stable_SSAO_In_BF3_With_STF.pdf
static float crossBilateralWeight(float offset, float depth, float centerDepth)
{
	const float sigma = (KERNEL_RADIUS + 1.f) * 0.5f;
	const float falloff = 1.f / (2.f * sigma * sigma);

	float dz = centerDepth - depth;
	return exp2(-offset * offset * falloff - dz * dz);
}

static void accumulate(float2 uv, float offset, float centerDepth, inout float total, inout float totalWeight)
{
	float value = input.SampleLevel(linearSampler, uv, 0);
	float depth = depthBuffer.SampleLevel(pointSampler, uv, 0);

	float weight = crossBilateralWeight(offset, depth, centerDepth);

	total += value * weight;
	totalWeight += weight;
}

static float blur(float2 direction, uint2 dispatchThreadID)
{
	direction *= cb.invDimensions;
	float2 centerUV = (dispatchThreadID + float2(0.5f, 0.5f)) * cb.invDimensions;

	float value = input.SampleLevel(linearSampler, centerUV, 0);
#if 1
	float depth = depthBuffer.SampleLevel(pointSampler, centerUV, 0);

	float total = value;
	float totalWeight = 1.f;

	float i = 1.f;
	for (; i <= KERNEL_RADIUS * 0.5f; i += 1.f)
	{
		accumulate(centerUV + i * direction, i, depth, total, totalWeight);
		accumulate(centerUV - i * direction, -i, depth, total, totalWeight);
	}

	for (; i <= KERNEL_RADIUS; i += 2.f)
	{
		accumulate(centerUV + (i + 0.5f) * direction, i, depth, total, totalWeight);
		accumulate(centerUV - (i + 0.5f) * direction, -i, depth, total, totalWeight);
	}

	value = total / totalWeight;
#endif
	return value;
}
