#include "light_probe_rs.hlsli"
#include "cs.hlsli"

ConstantBuffer<light_probe_update_cb> cb	: register(b0);
Texture2D<float3> radiance					: register(t0);
Texture2D<float4> directionAndDistance		: register(t1);

RWTexture2D<float3> output					: register(u0);



[numthreads(LIGHT_PROBE_BLOCK_SIZE, LIGHT_PROBE_BLOCK_SIZE, 1)]
[RootSignature(LIGHT_PROBE_UPDATE_RS)]
void main(cs_input IN)
{
	uint2 coord = IN.dispatchThreadID.xy;

	uint2 probeIndex2 = coord / LIGHT_PROBE_TOTAL_RESOLUTION;
	uint3 probeIndex3 = uint3(probeIndex2.x % cb.countX, probeIndex2.x / cb.countX, probeIndex2.y);

	uint probeIndex = probeIndex3.z * cb.countX * cb.countY + probeIndex3.y * cb.countX + probeIndex3.x;

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

	for (uint r = 0; r < NUM_RAYS_PER_PROBE; ++r)
	{
		uint2 c = uint2(r, probeIndex);
		
		float3 rad = radiance[c] * ENERGY_CONSERVATION;
		
		float4 dirDist = directionAndDistance[c];
		float3 rayDirection = dirDist.xyz;
		float rayDistance = dirDist.w;

		float weight = max(0.f, dot(rayDirection, pixelDirection));
		result += rad * weight;
		totalWeight += weight;
	}

	result *= 1.f / max(totalWeight, 1e-4f);

	const float hysteresis = 0.99f;


	const float3 previous = output[coord];
	float3 delta = result - previous;

#if 0
	const float maxStep = 0.1f;
	float sqStep = dot(delta, delta);
	if (sqStep > maxStep * maxStep)
	{
		delta = delta * maxStep / sqrt(sqStep);
	}
#endif

	float3 lerpDelta = (1.f - hysteresis) * delta;

	static const float c_threshold = 1.f / 1024.f;
	//lerpDelta = min(max(c_threshold, abs(lerpDelta)), abs(delta)) * sign(lerpDelta);
	
	output[coord] = previous + lerpDelta;// lerp(previous, result, 1.f - hysteresis);
}
