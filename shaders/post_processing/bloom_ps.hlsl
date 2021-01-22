#include "post_processing_rs.hlsli"

struct ps_input
{
	float2 uv	: TEXCOORDS;
};

static const float kernelOffsets[3] = { 0.f, 1.3846153846f, 3.2307692308f };
static const float blurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };



ConstantBuffer<bloom_cb> bloom	    : register(b0);

Texture2D<float4> src		        : register(t0);
SamplerState linearClampSampler     : register(s0);


[RootSignature(BLOOM_RS)]
float4 main(ps_input IN) : SV_TARGET
{
    float4 outColor = src.SampleLevel(linearClampSampler, IN.uv, 0) * blurWeights[0];

    [unroll]
    for (int i = 1; i < 3; ++i)
    {
        float2 normalizedOffset = kernelOffsets[i] * bloom.direction;
        outColor += src.SampleLevel(linearClampSampler, IN.uv + normalizedOffset, 0) * blurWeights[i];
        outColor += src.SampleLevel(linearClampSampler, IN.uv - normalizedOffset, 0) * blurWeights[i];
    }

    return outColor * bloom.multiplier;
}
