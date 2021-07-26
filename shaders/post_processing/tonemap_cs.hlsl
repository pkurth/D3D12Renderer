#include "cs.hlsli"
#include "post_processing_rs.hlsli"


ConstantBuffer<tonemap_cb> tonemap	: register(b0);
RWTexture2D<float4> output	: register(u0);
Texture2D<float4>	input	: register(t0);


[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(TONEMAP_RS)]
void main(cs_input IN)
{
	output[IN.dispatchThreadID.xy] = float4(
		tonemap.tonemap(
			input[IN.dispatchThreadID.xy].rgb
		)
		, 1.f);
}
