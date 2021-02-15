#include "post_processing_rs.hlsli"
#include "cs.hlsli"

ConstantBuffer<bloom_combine_cb> cb		: register(b0);

RWTexture2D<float4> output				: register(u0);
Texture2D<float4> scene					: register(t0);
Texture2D<float4> bloom					: register(t1);

SamplerState linearClampSampler			: register(s0);


[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(BLOOM_COMBINE_RS)]
void main(cs_input IN)
{
	const float2 uv = (IN.dispatchThreadID.xy + float2(0.5f, 0.5f)) * cb.invDimensions;

	float3 color = (float3)0.f;

	color += bloom.SampleLevel(linearClampSampler, uv, 1.5f).rgb;
	color += bloom.SampleLevel(linearClampSampler, uv, 3.5f).rgb;
	color += bloom.SampleLevel(linearClampSampler, uv, 4.5f).rgb;

	color /= 3.f;

	color *= cb.strength;

	output[IN.dispatchThreadID.xy] = float4(scene[IN.dispatchThreadID.xy].rgb + color, 1.f);
}
