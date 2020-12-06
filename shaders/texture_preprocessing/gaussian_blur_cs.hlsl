
#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 3), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE), UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)"

#include "cs.hlsl"

#define BLOCK_SIZE 16

cbuffer gaussian_blur_cb : register(b0)
{
    float2 direction; // [1, 0] or [0, 1], scaled by inverse screen dimensions.
};

static const float kernelOffsets[3] = { 0.0f, 1.3846153846f, 3.2307692308f };
static const float blurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };

Texture2D<float4> src		        : register(t0);
RWTexture2D<float4> dest		    : register(u0);
SamplerState linearClampSampler     : register(s0);


[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
    uint2 texCoords = IN.dispatchThreadID.xy;

    uint width, height, numMipLevels;
    src.GetDimensions(0, width, height, numMipLevels);

    if (texCoords.x >= width || texCoords.y >= height)
    {
        return;
    }

    float2 uv = (texCoords + float2(0.5f, 0.5f)) / float2(width, height);

    float4 outColor = src.SampleLevel(linearClampSampler, uv, 0) * blurWeights[0];

    for (int i = 1; i < 3; ++i)
    {
        float2 normalizedOffset = kernelOffsets[i] * direction;
        outColor += src.SampleLevel(linearClampSampler, uv + normalizedOffset, 0) * blurWeights[i];
        outColor += src.SampleLevel(linearClampSampler, uv - normalizedOffset, 0) * blurWeights[i];
    }

    dest[texCoords] = outColor;
}
