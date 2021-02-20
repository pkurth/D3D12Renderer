#include "post_processing_rs.hlsli"
#include "cs.hlsli"

ConstantBuffer<blit_cb> cb				: register(b0);
RWTexture2D<float4> output				: register(u0);
Texture2D<float4> input					: register(t0);
SamplerState linearClampSampler			: register(s0);

[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(BLIT_RS)]
void main(cs_input IN)
{
	const float2 uv = (IN.dispatchThreadID.xy + float2(0.5f, 0.5f)) * cb.invDimensions;
	output[IN.dispatchThreadID.xy] = input.SampleLevel(linearClampSampler, uv, 0);
}
