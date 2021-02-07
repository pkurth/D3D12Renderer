#include "cs.hlsli"
#include "post_processing_rs.hlsli"


// This is a 9x9 filter kernel.
static const float kernelOffsets[3] = { 0.f, 1.3846153846f, 3.2307692308f };
static const float blurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };

ConstantBuffer<gaussian_blur_cb> cb	    : register(b0);

Texture2D<float4> input		            : register(t0);
RWTexture2D<float4> output		        : register(u0);
SamplerState linearClampSampler         : register(s0);


[RootSignature(GAUSSIAN_BLUR_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(cs_input IN)
{
    float2 uv = (IN.dispatchThreadID.xy + float2(0.5f, 0.5f)) * cb.invDimensions;

    float2 direction = (cb.direction == 0) ? float2(1.f, 0.f) : float2(0.f, 1.f);
    direction *= cb.invDimensions;

    float4 color = input.SampleLevel(linearClampSampler, uv, cb.sourceMipLevel) * blurWeights[0];

    [unroll]
    for (int i = 1; i < 3; ++i)
    {
        float2 normalizedOffset = kernelOffsets[i] * direction;
        color += input.SampleLevel(linearClampSampler, uv + normalizedOffset, cb.sourceMipLevel) * blurWeights[i];
        color += input.SampleLevel(linearClampSampler, uv - normalizedOffset, cb.sourceMipLevel) * blurWeights[i];
    }

    output[IN.dispatchThreadID.xy] = color;
}
