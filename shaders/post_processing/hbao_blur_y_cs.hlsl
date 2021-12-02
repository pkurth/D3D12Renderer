#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "hbao_blur_common.hlsli"

//Texture2D<float2> motion            : register(t2);

[RootSignature(HBAO_BLUR_Y_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	float ao = blur(float2(0.f, 1.f), IN.dispatchThreadID.xy);
	output[IN.dispatchThreadID.xy] = ao;
}
