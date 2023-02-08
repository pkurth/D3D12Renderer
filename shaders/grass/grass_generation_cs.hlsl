#include "cs.hlsli"
#include "grass_rs.hlsli"
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

static const float cullCutoffs[2][2] =
{
	{ 2.f, 0.25f },
	{ 0.75f, 0.8f },
};

[RootSignature(GRASS_GENERATION_RS)]
[numthreads(GRASS_GENERATION_BLOCK_SIZE, GRASS_GENERATION_BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	uint seed = initRand(IN.dispatchThreadID.x << cb.lodIndex, IN.dispatchThreadID.y << cb.lodIndex);

	float2 uv = (IN.dispatchThreadID.xy + 0.5f) * cb.uvScale;
	float uvOffsetX = (nextRand(seed) * 2.f - 1.f) * cb.uvScale;
	float uvOffsetZ = (nextRand(seed) * 2.f - 1.f) * cb.uvScale;
	uv += float2(uvOffsetX, uvOffsetZ);

	if (isSaturated(uv))
	{
		float height = heightmap.SampleLevel(clampSampler, uv, 0) * common.amplitudeScale;
		float2 n = normalmap.SampleLevel(clampSampler, uv, 0) * common.amplitudeScale;
		float3 normal = normalize(float3(n.x, 1.f, n.y));


		float2 xz = uv * common.chunkSize;

		float3 position = float3(xz.x, height, xz.y) + cb.chunkCorner;

		if (normal.y > 0.9f && !cull(position - float3(0.5f, 0.f, 0.5f), position + float3(0.5f, 1.5f, 0.5f)))
		{
			bool cull = false;
			float lod = 0.f;

			if (cb.lodIndex == 0)
			{
				float distance = length(position - common.cameraPosition);
				float cullValue = smoothstep(common.lodChangeStartDistance, common.lodChangeEndDistance, distance) - nextRand(seed) * 0.1f;

				float cutoff = cullCutoffs[IN.dispatchThreadID.x & 1][IN.dispatchThreadID.y & 1];

				cull = cullValue >= cutoff;
				lod = cullValue;
			}

			if (!cull)
			{
				uint lodIndex = cb.lodIndex;

				grass_blade blade;
				blade.initialize(
					position,
					random(xz) * M_PI * 2.f,
					0, // TODO: Type.
					lod);

				uint index;
				InterlockedAdd(count[lodIndex], 1, index);
				blades[lodIndex][index] = blade;
			}
		}
	}
}
