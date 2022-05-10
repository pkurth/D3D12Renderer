#include "light_probe_rs.hlsli"

struct ps_input
{
	float2 uv				: TEXCOORDS;
	float4 screenPosition	: SV_POSITION;
};

ConstantBuffer<light_probe_update_cb> cb	: register(b0);
Texture2D<float3> radiance					: register(t0);
Texture2D<float4> directionAndDistance		: register(t1);


[RootSignature(LIGHT_PROBE_UPDATE_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	uint2 coord = (uint2)IN.screenPosition.xy;

	uint2 probeIndex2 = coord / LIGHT_PROBE_TOTAL_RESOLUTION;
	uint3 probeIndex3 = uint3(probeIndex2.x % cb.countX, probeIndex2.x / cb.countX, probeIndex2.y);

	uint probeIndex = probeIndex3.z * cb.countX * cb.countY + probeIndex3.y * cb.countX + probeIndex3.x;

	uint2 pixelIndex = coord % LIGHT_PROBE_TOTAL_RESOLUTION; // [1, 6]

	if (pixelIndex.x == 0) 
	{ 
		pixelIndex.x = 1;
		pixelIndex.y = LIGHT_PROBE_TOTAL_RESOLUTION - pixelIndex.y - 1;
	}
	if (pixelIndex.x == LIGHT_PROBE_TOTAL_RESOLUTION - 1)
	{
		pixelIndex.x = LIGHT_PROBE_TOTAL_RESOLUTION - 2;
		pixelIndex.y = LIGHT_PROBE_TOTAL_RESOLUTION - pixelIndex.y - 1;
	}
	if (pixelIndex.y == 0)
	{
		pixelIndex.y = 1;
		pixelIndex.x = LIGHT_PROBE_TOTAL_RESOLUTION - pixelIndex.x - 1;
	}
	if (pixelIndex.y == LIGHT_PROBE_TOTAL_RESOLUTION - 1)
	{
		pixelIndex.y = LIGHT_PROBE_TOTAL_RESOLUTION - 2;
		pixelIndex.x = LIGHT_PROBE_TOTAL_RESOLUTION - pixelIndex.x - 1;
	}


	pixelIndex -= 1; // Subtract the border -> [0, 5]

	float2 uv = ((float2)pixelIndex + 0.5f) / LIGHT_PROBE_RESOLUTION;
	float2 oct = uv * 2.f - 1.f;

	float3 pixelDirection = decodeOctahedral(oct);

	float4 irradiance = float4(0.f, 0.f, 0.f, 0.f);

	for (uint r = 0; r < NUM_RAYS_PER_PROBE; ++r)
	{
		uint2 c = uint2(r, probeIndex);
		
		float3 rad = radiance[c];
		
		float4 dirDist = directionAndDistance[c];
		float3 rayDirection = dirDist.xyz;
		float rayDistance = dirDist.w;

		float weight = max(0.f, dot(rayDirection, pixelDirection));
		irradiance += float4(rad * weight, weight);
	}

	if (irradiance.w > 1e-3f)
	{
		irradiance.xyz /= irradiance.w;
		irradiance.w = 0.1f;
	}

	return irradiance;
}
