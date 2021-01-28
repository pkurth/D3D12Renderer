#include "post_processing_rs.hlsli"
#include "cs.hlsli"

#define BLOCK_SIZE 32

ConstantBuffer<taa_cb> cb			: register(b0);

RWTexture2D<float4> currentFrame	: register(u0);
Texture2D<float4> prevFrame			: register(t0);
Texture2D<float2> motion			: register(t1);

SamplerState linearSampler			: register(s0);


static const int2 offsets[] = {
	int2(-1, -1),
	int2(0, -1),
	int2(1, -1),
	int2(-1, 0),
	int2(1, 0),
	int2(-1, 1),
	int2(0, 1),
	int2(1, 1),
};


[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
[RootSignature(TAA_RS)]
void main(cs_input IN)
{
	int2 texCoord = IN.dispatchThreadID.xy;
	if (texCoord.x >= cb.dimensions.x || texCoord.y >= cb.dimensions.y)
	{
		return;
	}

	float2 invDimensions = 1.f / cb.dimensions;
	float2 uv = (float2(texCoord) + float2(0.5f, 0.5f)) * invDimensions;

	float4 current = currentFrame[texCoord];

	float3 rgbMin = current.xyz;
	float3 rgbMax = current.xyz;

	[unroll]
	for (uint i = 0; i < 8; ++i)
	{
		float3 c = currentFrame[texCoord + offsets[i]].xyz;
		rgbMin = min(rgbMin, c);
		rgbMax = max(rgbMax, c);
	}

	float2 prevUV = uv + motion[texCoord];
	float4 prev = prevFrame.SampleLevel(linearSampler, prevUV, 0);
	prev.xyz = clamp(prev.xyz, rgbMin, rgbMax);

	currentFrame[texCoord] = lerp(prev, current, 0.05f);
}
