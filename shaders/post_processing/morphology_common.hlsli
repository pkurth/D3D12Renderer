#include "cs.hlsli"
#include "post_processing_rs.hlsli"

#ifndef DATA_TYPE
#define DATA_TYPE float
#endif

#define MAX_SHARED_MEM_WIDTH (MORPHOLOGY_BLOCK_SIZE + MORPHOLOGY_MAX_RADIUS * 2)

ConstantBuffer<morphology_cb> cb	: register(b0);
RWTexture2D<DATA_TYPE> output		: register(u0);
Texture2D<DATA_TYPE> input			: register(t0);

groupshared DATA_TYPE g_values[MAX_SHARED_MEM_WIDTH];

[numthreads(MORPHOLOGY_BLOCK_SIZE, 1, 1)]
[RootSignature(MORPHOLOGY_RS)]
void main(cs_input IN)
{
	const int direction = cb.direction;
	const int radius = cb.radius;
	const int dimInDirection = cb.dimInDirection;
	const int actualSharedMemoryWidth = MORPHOLOGY_BLOCK_SIZE + radius * 2;

	const int y = IN.dispatchThreadID.y;
	const int tileStart = IN.groupID.x * MORPHOLOGY_BLOCK_SIZE;
	const int paddedTileStart = tileStart - radius;

	const int blockOverlapLeft = -min(paddedTileStart, 0);
	const int blockEnd = actualSharedMemoryWidth - 1;
	const int blockOverlapRight = max(paddedTileStart + blockEnd + 1 - dimInDirection, 0);

	for (int t = IN.groupIndex + blockOverlapLeft; t <= blockEnd - blockOverlapRight; t += MORPHOLOGY_BLOCK_SIZE)
	{
		int x = paddedTileStart + t;
		int2 pos = int2(x, y);
		pos = (direction == 0) ? pos.xy : pos.yx;
		g_values[t] = input[pos];
	}

	GroupMemoryBarrierWithGroupSync();



	DATA_TYPE value = NULL_VALUE;

	const int globalStart = IN.dispatchThreadID.x - radius;
	const int overlapLeft = -min(globalStart, 0);

	const int globalEnd = IN.dispatchThreadID.x + radius;
	const int overlapRight = max(globalEnd + 1 - dimInDirection, 0);

	const int start = IN.groupIndex + overlapLeft;
	const int end = IN.groupIndex + radius * 2 - overlapRight;
	for (int i = start; i <= end; ++i)
	{
		value = OP(value, g_values[i]);
	}

	int2 outPos = IN.dispatchThreadID.xy;
	outPos = (direction == 0) ? outPos.xy : outPos.yx;
	output[outPos] = value;
}



