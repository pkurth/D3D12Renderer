#include "cs.hlsli"
#include "proc_placement_rs.hlsli"

ConstantBuffer<prefix_sum_cb> cb	: register(b0);
RWStructuredBuffer<uint> output		: register(u0);
StructuredBuffer<uint> input		: register(t0);

groupshared uint temp[512];

[RootSignature(PROC_PLACEMENT_PREFIX_SUM_RS)]
[numthreads(512, 1, 1)]
void main(cs_input IN)
{
	uint id = IN.dispatchThreadID.x;

	uint x = (id < cb.size) ? input[id] : 0;
	temp[id] = x;

	uint sum = x;

	[unroll]
	for (uint offset = 1; offset < 512; offset <<= 1)
	{
		GroupMemoryBarrierWithGroupSync();

		if (id >= offset)
		{
			sum += temp[id - offset];
		}

		temp[id] = sum;
	}

	if (id < cb.size)
	{
		output[id] = sum - x; // Subtract original value x, since we perform an exclusive sum.
	}
}
