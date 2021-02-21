#include "cs.hlsli"
#include "post_processing_rs.hlsli"


ConstantBuffer<gaussian_blur_cb> cb	    : register(b0);

Texture2D<float4> input		            : register(t0);
RWTexture2D<float4> output		        : register(u0);
SamplerState linearClampSampler         : register(s0);


[RootSignature(GAUSSIAN_BLUR_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(cs_input IN)
{
    float2 uv = (IN.dispatchThreadID.xy + float2(0.5f, 0.5f)) * cb.invDimensions;

    uint directionIndex = cb.directionAndSourceMipLevel >> 16;
    float2 direction = (directionIndex == 0) ? float2(1.f, 0.f) : float2(0.f, 1.f);
    direction *= cb.invDimensions;
    direction *= cb.stepScale;

    uint sourceMipLevel = cb.directionAndSourceMipLevel & 0xFFFF;
    float4 color = input.SampleLevel(linearClampSampler, uv, sourceMipLevel) * blurWeights[0];

    [unroll]
    for (int i = 1; i < NUM_WEIGHTS; ++i)
    {
        float2 normalizedOffset = kernelOffsets[i] * direction;
        color += input.SampleLevel(linearClampSampler, uv + normalizedOffset, sourceMipLevel) * blurWeights[i];
        color += input.SampleLevel(linearClampSampler, uv - normalizedOffset, sourceMipLevel) * blurWeights[i];
    }

    output[IN.dispatchThreadID.xy] = color;
}
