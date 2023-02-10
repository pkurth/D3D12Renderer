#include "cs.hlsli"
#include "proc_placement_rs.hlsli"
#include "math.hlsli"
#include "random.hlsli"

ConstantBuffer<proc_placement_generate_points_cb> cb	: register(b0);

Texture2D<float> heightmap								: register(t0);
Texture2D<float2> normalmap								: register(t1);

RWStructuredBuffer<placement_point> placementPoints		: register(u0);
RWStructuredBuffer<uint> pointAndMeshCount				: register(u1);

SamplerState clampSampler								: register(s0);



[RootSignature(PROC_PLACEMENT_GENERATE_POINTS_RS)]
[numthreads(32, 32, 1)]
void main(cs_input IN)
{
	float2 samplePoint = POISSON_SAMPLES[IN.groupIndex];
	float2 uv = (samplePoint + IN.groupID.xy) * cb.uvScale;

	float height = 0.f;
	float3 normal = float3(0.f, 1.f, 0.f);

	if (isSaturated(uv))
	{
		height = heightmap.SampleLevel(clampSampler, uv, 0) * cb.amplitudeScale;
		float2 n = normalmap.SampleLevel(clampSampler, uv, 0) * cb.amplitudeScale;
		normal = normalize(float3(n.x, 1.f, n.y));

		if (normal.y > 0.9f)
		{
			float2 xz = uv * cb.chunkSize;
			float3 position = float3(xz.x, height, xz.y) + cb.chunkCorner;


			float4 densities = cb.densities;


			float densitySum = dot(densities, (float4)1.f);

			densitySum = max(densitySum, 1.f); // Only normalize if we are above 1.
			densities *= 1.f / densitySum;

			vec4 unusedChannels = vec4(cb.numMeshes < 1, cb.numMeshes < 2, cb.numMeshes < 3, cb.numMeshes < 4);
			densities += unusedChannels; // Initialize unused channels to high.

			densities.y += densities.x;
			densities.z += densities.y;
			densities.w += densities.z;




			float threshold = fbm(uv * 1.5f, 4).x * 0.5f + 0.5f;

			float4 comparison = (float4)threshold > densities;
			uint meshIndex = (uint)dot(comparison, comparison);

			//uint meshIndex = (uint)(random(xz) * cb.numMeshes - 0.001f);
			
			
			uint globalMeshIndex = cb.globalMeshOffset + meshIndex;

			uint lodIndex = 0; // TODO

			uint offset;
			InterlockedAdd(pointAndMeshCount[0], 1, offset);
			InterlockedAdd(pointAndMeshCount[globalMeshIndex], 1);

			placement_point result;
			result.position = position;
			result.normal = normal;
			result.meshID = globalMeshIndex;
			result.lod = lodIndex;
			placementPoints[offset] = result;
		}
	}
}
