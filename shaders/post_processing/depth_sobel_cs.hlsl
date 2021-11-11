#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "camera.hlsli"


ConstantBuffer<depth_sobel_cb> cb	: register(b0);
RWTexture2D<float> output			: register(u0);
Texture2D<float> input				: register(t0);

[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(DEPTH_SOBEL_RS)]
void main(cs_input IN)
{
	int2 xy = IN.dispatchThreadID.xy;

	float tl = depthBufferDepthToEyeDepth(input[xy + int2(-1, -1)], cb.projectionParams);
	float t  = depthBufferDepthToEyeDepth(input[xy + int2(0, -1)], cb.projectionParams);
	float tr = depthBufferDepthToEyeDepth(input[xy + int2(1, -1)], cb.projectionParams);
	float l  = depthBufferDepthToEyeDepth(input[xy + int2(-1, 0)], cb.projectionParams);
	float r  = depthBufferDepthToEyeDepth(input[xy + int2(1, 0)], cb.projectionParams);
	float bl = depthBufferDepthToEyeDepth(input[xy + int2(-1, 1)], cb.projectionParams);
	float b  = depthBufferDepthToEyeDepth(input[xy + int2(0, 1)], cb.projectionParams);
	float br = depthBufferDepthToEyeDepth(input[xy + int2(1, 1)], cb.projectionParams);

	float horizontal = abs((tl + 2.f * t + tr) - (bl + 2.f * b + br));
	float vertical   = abs((tl + 2.f * l + bl) - (tr + 2.f * r + br));

	float edge = (horizontal > cb.threshold || vertical > cb.threshold) ? 1.f : 0.f;
	output[xy] = edge;
}

