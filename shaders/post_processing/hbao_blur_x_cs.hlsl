#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "hbao_blur_common.hlsli"

[RootSignature(HBAO_BLUR_X_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	float ao = blur(float2(1.f, 0.f), IN.dispatchThreadID.xy);
	output[IN.dispatchThreadID.xy] = ao;
}
