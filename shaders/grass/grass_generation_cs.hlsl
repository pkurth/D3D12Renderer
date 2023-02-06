#include "cs.hlsli"
#include "grass_rs.hlsli"
#include "proc_placement_rs.hlsli"
#include "math.hlsli"
#include "random.hlsli"

ConstantBuffer<grass_generation_cb> cb	: register(b0);

Texture2D<float> heightmap				: register(t0);
Texture2D<float2> normalmap				: register(t1);

RWStructuredBuffer<grass_blade> blades	: register(u0);
RWStructuredBuffer<uint> count			: register(u1);

SamplerState clampSampler				: register(s0);


groupshared uint groupCount;
groupshared uint groupStartOffset;


[RootSignature(GRASS_GENERATION_RS)]
[numthreads(32, 32, 1)]
void main(cs_input IN)
{
	if (IN.groupIndex == 0)
	{
		groupCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	float2 samplePoint = POISSON_SAMPLES[IN.groupIndex];
	float2 uv = (samplePoint + IN.groupID.xy) * cb.uvScale;

	float height = 0.f;
	float3 normal = float3(0.f, 1.f, 0.f);

	uint valid = 0;
	if (isSaturated(uv))
	{
		height = heightmap.SampleLevel(clampSampler, uv, 0) * cb.amplitudeScale;
		float2 n = normalmap.SampleLevel(clampSampler, uv, 0) * cb.amplitudeScale;
		normal = normalize(float3(n.x, 1.f, n.y));

		if (normal.y > 0.9f)
		{
			valid = 1;
		}
	}


	uint innerGroupIndex;
	InterlockedAdd(groupCount, valid, innerGroupIndex);



	GroupMemoryBarrierWithGroupSync();

	if (IN.groupIndex == 0)
	{
		InterlockedAdd(count[0], groupCount, groupStartOffset);
	}

	GroupMemoryBarrierWithGroupSync();



	if (valid)
	{
		float2 xz = uv * cb.chunkSize;

		grass_blade blade;
		blade.position = float3(xz.x, height, xz.y) + cb.chunkCorner;
		blade.type = 0;
		
		float angle = random(xz);
		sincos(angle, blade.facing.x, blade.facing.y);

		blades[groupStartOffset + innerGroupIndex] = blade;
	}
}
