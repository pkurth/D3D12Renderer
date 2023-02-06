#include "cs.hlsli"
#include "grass_rs.hlsli"
#include "proc_placement_rs.hlsli"
#include "math.hlsli"
#include "random.hlsli"

ConstantBuffer<grass_generation_cb> cb				: register(b0);
ConstantBuffer<grass_generation_common_cb> common	: register(b1);

Texture2D<float> heightmap							: register(t0);
Texture2D<float2> normalmap							: register(t1);

RWStructuredBuffer<grass_blade> blades[2]			: register(u0);
RWStructuredBuffer<uint> count						: register(u2);

SamplerState clampSampler							: register(s0);

// Returns true, if object should be culled.
static bool cull(float3 minCorner, float3 maxCorner)
{
	for (uint32 i = 0; i < 6; ++i)
	{
		float4 plane = common.frustumPlanes[i];
		float4 vertex = float4(
			(plane.x < 0.f) ? minCorner.x : maxCorner.x,
			(plane.y < 0.f) ? minCorner.y : maxCorner.y,
			(plane.z < 0.f) ? minCorner.z : maxCorner.z,
			1.f
			);
		if (dot(plane, vertex) < 0.f)
		{
			return true;
		}
	}
	return false;
}

struct lod_decision
{
	float lod;
	bool cull;
};

static lod_decision decideLOD(float3 position)
{
	float rand = random(position);
	float distance = length(position - common.cameraPosition);

	float lod_01 = smoothstep(150.f, 180.f, distance + rand * 20.f);
	float lod_1cull = smoothstep(240.f, 270.f, distance + rand * 20.f);

	float totalLOD = lod_01 + lod_1cull;

	lod_decision result = { totalLOD, totalLOD >= 2.f };
	return result;
}


[RootSignature(GRASS_GENERATION_RS)]
[numthreads(32, 32, 1)]
void main(cs_input IN)
{
	float2 samplePoint = POISSON_SAMPLES[IN.groupIndex];
	float2 uv = (samplePoint + IN.groupID.xy) * common.uvScale;

	if (isSaturated(uv))
	{
		float height = heightmap.SampleLevel(clampSampler, uv, 0) * common.amplitudeScale;
		float2 n = normalmap.SampleLevel(clampSampler, uv, 0) * common.amplitudeScale;
		float3 normal = normalize(float3(n.x, 1.f, n.y));


		float2 xz = uv * common.chunkSize;

		float3 position = float3(xz.x, height, xz.y) + cb.chunkCorner;

		if (normal.y > 0.95f && !cull(position - float3(0.2f, 0.f, 0.2f), position + float3(0.2f, 1.f, 0.2f)))
		{
			lod_decision lod = decideLOD(position);

			if (!lod.cull)
			{
				grass_blade blade;
				blade.initialize(
					position,
					random(xz) * M_PI * 2.f,
					0,
					lod.lod > 1.f ? 0.f : lod.lod);

				uint lodIndex = (uint)lod.lod;

				uint index;
				InterlockedAdd(count[lodIndex], 1, index);
				blades[lodIndex][index] = blade;
			}
		}
	}
}
