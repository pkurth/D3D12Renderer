#include "post_processing_rs.hlsli"
#include "cs.hlsli"

ConstantBuffer<bloom_threshold_cb> cb	: register(b0);

RWTexture2D<float4> output				: register(u0);
Texture2D<float4> input					: register(t0);

SamplerState linearClampSampler			: register(s0);


[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(BLOOM_THRESHOLD_RS)]
void main(cs_input IN)
{
	const float2 uv = IN.dispatchThreadID.xy + float2(0.5f, 0.5f);

	float3 color = 0.xxx;

	const float2 invDimensions = cb.invDimensions;
	color += input.SampleLevel(linearClampSampler, (uv + float2(-1, -1)) * invDimensions, 0).rgb;
	color += input.SampleLevel(linearClampSampler, (uv + float2(1, -1)) * invDimensions, 0).rgb;
	color += input.SampleLevel(linearClampSampler, (uv + float2(-1, 1)) * invDimensions, 0).rgb;
	color += input.SampleLevel(linearClampSampler, (uv + float2(1, 1)) * invDimensions, 0).rgb;

	color *= 0.25f;
	color = max(0, color - cb.threshold.xxx);

	output[IN.dispatchThreadID.xy] = float4(color, 1.f);
}
