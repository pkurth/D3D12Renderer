#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "camera.hlsli"

// This shader outputs the depth in world units!

ConstantBuffer<hierarchical_linear_depth_cb> cb	    : register(b0);

RWTexture2D<float> outputMip0						: register(u0);
RWTexture2D<float> outputMip1						: register(u1);
RWTexture2D<float> outputMip2						: register(u2);
RWTexture2D<float> outputMip3						: register(u3);
RWTexture2D<float> outputMip4						: register(u4);
RWTexture2D<float> outputMip5						: register(u5);

Texture2D<float> input								: register(t0);

SamplerState linearClampSampler						: register(s0);



groupshared float tile[POST_PROCESSING_BLOCK_SIZE][POST_PROCESSING_BLOCK_SIZE];


[RootSignature(HIERARCHICAL_LINEAR_DEPTH_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	const uint2 groupThreadID = IN.groupThreadID.xy;
	const uint2 dispatchThreadID = IN.dispatchThreadID.xy;

	float2 uv = (dispatchThreadID + float2(0.5f, 0.5f)) * cb.invDimensions;

	float4 depths = input.Gather(linearClampSampler, uv);
	//float4 depths = float4(
	//	input[dispatchThreadID * 2 + uint2(0, 0)],
	//	input[dispatchThreadID * 2 + uint2(1, 0)],
	//	input[dispatchThreadID * 2 + uint2(0, 1)],
	//	input[dispatchThreadID * 2 + uint2(1, 1)]
	//);
	float maxdepth = max(depths.x, max(depths.y, max(depths.z, depths.w)));
	tile[groupThreadID.x][groupThreadID.y] = maxdepth;

	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 2 == 0 && groupThreadID.y % 2 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], 
			max(tile[groupThreadID.x + 1][groupThreadID.y], 
			max(tile[groupThreadID.x][groupThreadID.y + 1], 
				tile[groupThreadID.x + 1][groupThreadID.y + 1])));
		tile[groupThreadID.x][groupThreadID.y] = maxdepth;
	}
	GroupMemoryBarrierWithGroupSync();



	const float4 lineardepths = float4
	(
		depthBufferDepthToEyeDepth(depths.x, cb.projectionParams),
		depthBufferDepthToEyeDepth(depths.y, cb.projectionParams),
		depthBufferDepthToEyeDepth(depths.z, cb.projectionParams),
		depthBufferDepthToEyeDepth(depths.w, cb.projectionParams)
	);

	outputMip0[dispatchThreadID * 2 + uint2(0, 0)] = lineardepths.x;
	outputMip0[dispatchThreadID * 2 + uint2(1, 0)] = lineardepths.y;
	outputMip0[dispatchThreadID * 2 + uint2(0, 1)] = lineardepths.z;
	outputMip0[dispatchThreadID * 2 + uint2(1, 1)] = lineardepths.w;

	maxdepth = max(lineardepths.x, max(lineardepths.y, max(lineardepths.z, lineardepths.w)));
	tile[groupThreadID.x][groupThreadID.y] = maxdepth;
	outputMip1[dispatchThreadID] = maxdepth;
	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 2 == 0 && groupThreadID.y % 2 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], max(tile[groupThreadID.x + 1][groupThreadID.y], max(tile[groupThreadID.x][groupThreadID.y + 1], tile[groupThreadID.x + 1][groupThreadID.y + 1])));
		tile[groupThreadID.x][groupThreadID.y] = maxdepth;
		outputMip2[dispatchThreadID / 2] = maxdepth;
	}
	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 4 == 0 && groupThreadID.y % 4 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], max(tile[groupThreadID.x + 2][groupThreadID.y], max(tile[groupThreadID.x][groupThreadID.y + 2], tile[groupThreadID.x + 2][groupThreadID.y + 2])));
		tile[groupThreadID.x][groupThreadID.y] = maxdepth;
		outputMip3[dispatchThreadID / 4] = maxdepth;
	}
	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 8 == 0 && groupThreadID.y % 8 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], max(tile[groupThreadID.x + 4][groupThreadID.y], max(tile[groupThreadID.x][groupThreadID.y + 4], tile[groupThreadID.x + 4][groupThreadID.y + 4])));
		tile[groupThreadID.x][groupThreadID.y] = maxdepth;
		outputMip4[dispatchThreadID / 8] = maxdepth;
	}
	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 16 == 0 && groupThreadID.y % 16 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], max(tile[groupThreadID.x + 8][groupThreadID.y], max(tile[groupThreadID.x][groupThreadID.y + 8], tile[groupThreadID.x + 8][groupThreadID.y + 8])));
		outputMip5[dispatchThreadID / 16] = maxdepth;
	}
}
