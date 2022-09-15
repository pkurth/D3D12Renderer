#include "light_probe_rs.hlsli"
#include "cs.hlsli"

ConstantBuffer<light_probe_update_cb> cb	: register(b0);
Texture2D<float3> radiance					: register(t0);
Texture2D<float4> directionAndDistance		: register(t1);

RWTexture2D<float3> output					: register(u0);

#define USE_SHARED_MEMORY 1

#if USE_SHARED_MEMORY
groupshared float3 g_radiance[NUM_RAYS_PER_PROBE];
groupshared float4 g_directionAndDistance[NUM_RAYS_PER_PROBE];
#endif

[numthreads(LIGHT_PROBE_BLOCK_SIZE, LIGHT_PROBE_BLOCK_SIZE, 1)]
[RootSignature(LIGHT_PROBE_UPDATE_RS)]
void main(cs_input IN)
{
	uint2 coord = IN.dispatchThreadID.xy;

	uint2 probeIndex2 = IN.groupID.xy;// coord / LIGHT_PROBE_TOTAL_RESOLUTION;
	uint3 probeIndex3 = uint3(probeIndex2.x % cb.countX, probeIndex2.x / cb.countX, probeIndex2.y);

	uint probeIndex = probeIndex3.z * cb.countX * cb.countY + probeIndex3.y * cb.countX + probeIndex3.x;

#if USE_SHARED_MEMORY
	[unroll]
	for (uint i = IN.groupIndex; i < NUM_RAYS_PER_PROBE; i += (LIGHT_PROBE_BLOCK_SIZE * LIGHT_PROBE_BLOCK_SIZE))
	{
		uint2 c = uint2(i, probeIndex);
		g_radiance[i] = radiance[c];
		g_directionAndDistance[i] = directionAndDistance[c];
	}
	GroupMemoryBarrierWithGroupSync();
#endif


	uint2 pixelIndex = coord % LIGHT_PROBE_TOTAL_RESOLUTION; // [1, 6]


	// Map borders to wrapping interior texel.
	pixelIndex = (pixelIndex.x == 0) ? uint2(1, LIGHT_PROBE_TOTAL_RESOLUTION - pixelIndex.y - 1) : pixelIndex;
	pixelIndex = (pixelIndex.x == LIGHT_PROBE_TOTAL_RESOLUTION - 1) ? uint2(LIGHT_PROBE_TOTAL_RESOLUTION - 2, LIGHT_PROBE_TOTAL_RESOLUTION - pixelIndex.y - 1) : pixelIndex;
	pixelIndex = (pixelIndex.y == 0) ? uint2(LIGHT_PROBE_TOTAL_RESOLUTION - pixelIndex.x - 1, 1) : pixelIndex;
	pixelIndex = (pixelIndex.y == LIGHT_PROBE_TOTAL_RESOLUTION - 1) ? uint2(LIGHT_PROBE_TOTAL_RESOLUTION - pixelIndex.x - 1, LIGHT_PROBE_TOTAL_RESOLUTION - 2) : pixelIndex;


	pixelIndex -= 1; // Subtract the border -> [0, 5]

	float2 uv = ((float2)pixelIndex + 0.5f) / LIGHT_PROBE_RESOLUTION;
	float2 oct = uv * 2.f - 1.f;

	float3 pixelDirection = decodeOctahedral(oct);

	float3 result = float3(0.f, 0.f, 0.f);
	float totalWeight = 0.f;

	for (uint j = 0; j < NUM_RAYS_PER_PROBE; ++j)
	{
#if USE_SHARED_MEMORY
		float3 rad = g_radiance[j];
		float4 dirDist = g_directionAndDistance[j];
#else
		uint2 c = uint2(j, probeIndex);
		float3 rad = radiance[c];
		float4 dirDist = directionAndDistance[c];
#endif

		float3 rayDirection = dirDist.xyz;
		float rayDistance = dirDist.w;

		float weight = max(0.f, dot(rayDirection, pixelDirection));
		result += rad * weight;
		totalWeight += weight;
	}

	result *= ENERGY_CONSERVATION / max(totalWeight, 1e-4f);

	float hysteresis = 0.99f;

	const float3 previous = output[coord];

	if (dot(previous, previous) == 0.f)
	{
		hysteresis = 0.f; // Speed up first frame's convergence.
	}

	output[coord] = lerp(previous, result, 1.f - hysteresis);
}
