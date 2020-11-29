
#define RS \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 3), " \
    "DescriptorTable( SRV(t0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE), UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )," \
    "SRV(t1)," \
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
    int halfKernelSize;
};

Texture2D<float4> src		    : register(t0);
StructuredBuffer<float> weights : register(t1);

SamplerState linearClampSampler : register(s0);

RWTexture2D<float4> dest		: register(u0);

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

    float4 outColor = src.SampleLevel(linearClampSampler, uv, 0) * weights[0];

    for (int i = 1; i < halfKernelSize; i += 2)
    {
        int i0 = i;
        int i1 = i + 1;
        float w0 = weights[i0];
        float w1 = weights[i1];
        float weight = w0 + w1;
        float offset = (i0 * w0 + i1 * w1) / weight;

        outColor += src.SampleLevel(linearClampSampler, uv + offset * direction, 0) * weight;
        outColor += src.SampleLevel(linearClampSampler, uv - offset * direction, 0) * weight;
    }

    dest[texCoords] = outColor;
}
