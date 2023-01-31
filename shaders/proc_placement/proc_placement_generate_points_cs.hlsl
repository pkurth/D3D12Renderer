#include "cs.hlsli"
#include "proc_placement_rs.hlsli"
#include "math.hlsli"

ConstantBuffer<proc_placement_generate_points_cb> cb	: register(b0);

Texture2D<float> heightmap								: register(t0);
Texture2D<float2> normalmap								: register(t1);

RWStructuredBuffer<placement_point> placementPoints		: register(u0);
RWStructuredBuffer<uint> pointCount						: register(u1);
RWStructuredBuffer<uint> submeshCount					: register(u2);

SamplerState clampSampler								: register(s0);



groupshared uint groupCount;
groupshared uint groupStartOffset;


[RootSignature(PROC_PLACEMENT_GENERATE_POINTS_RS)]
[numthreads(32, 32, 1)]
void main(cs_input IN)
{
	if (IN.groupIndex == 0)
	{
		groupCount = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	float2 samplePoint = POISSON_SAMPLES[IN.groupIndex];
	float2 uv = samplePoint * cb.uvScale + IN.groupID.xy * cb.uvStride;

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
		InterlockedAdd(pointCount[0], groupCount, groupStartOffset);
	}

	GroupMemoryBarrierWithGroupSync();



	if (valid)
	{
		float2 xz = uv * cb.chunkSize;
		float3 position = float3(xz.x, height, xz.y) + cb.chunkCorner;


		uint lodIndex = 0;
		uint meshIndex = 0;


		uint firstSubmesh = cb.firstSubmeshPerLayerObject[meshIndex];
		uint numSubmeshes = cb.numSubmeshesPerLayerObject[meshIndex];

		for (uint i = firstSubmesh; i < firstSubmesh + numSubmeshes; ++i)
		{
			InterlockedAdd(submeshCount[i], 1);
		}


		placement_point result;
		result.position = position;
		result.normal = normal;
		result.meshID = meshIndex;
		result.lod = lodIndex;
		placementPoints[groupStartOffset + innerGroupIndex] = result;
	}
}
