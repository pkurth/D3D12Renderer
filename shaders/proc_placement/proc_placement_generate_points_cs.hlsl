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
[numthreads(16, 16, 1)]
void main(cs_input IN)
{
	//float2 uv = (POISSON_SAMPLES[IN.groupIndex] + IN.groupID.xy) * cb.uvScale;
	float2 localUV = (IN.groupThreadID.xy + 0.5f) * (1.f / 16.f);
	float2 uv = (localUV + IN.groupID.xy) * cb.uvScale;

	if (isSaturated(uv))
	{
		float height = heightmap.SampleLevel(clampSampler, uv, 0) * cb.amplitudeScale;
		float2 n = normalmap.SampleLevel(clampSampler, uv, 0) * cb.amplitudeScale;
		float3 normal = normalize(float3(n.x, 1.f, n.y));

		if (normal.y > 0.9f)
		{
			float4 densities = cb.densities;
			
			float densitySum = dot(densities, (float4)1.f);
			
			densitySum = max(densitySum, 1.f); // Only normalize if we are above 1.
			densities *= 1.f / densitySum;
			
			float4 unusedChannels = float4(cb.numMeshes < 1, cb.numMeshes < 2, cb.numMeshes < 3, cb.numMeshes < 4);
			densities += unusedChannels; // Initialize unused channels to high.
			
			densities.y += densities.x;
			densities.z += densities.y;
			densities.w += densities.z;



			float threshold = fbm(uv * 1.5f, 4).x * 0.5f + 0.5f;
			
			float4 comparison = (float4)threshold > densities;
			uint meshIndex = (uint)dot(comparison, comparison);

			uint globalMeshIndex = cb.globalMeshOffset + meshIndex;

			uint offset;
			InterlockedAdd(pointAndMeshCount[0], 1, offset);
			InterlockedAdd(pointAndMeshCount[globalMeshIndex], 1);

			float2 xz = uv * cb.chunkSize;

			placement_point result;
			result.position = float3(xz.x, height, xz.y) + cb.chunkCorner;
			result.normal = normal;
			result.meshID = globalMeshIndex;
			result.lod = 0; // TODO
			placementPoints[offset] = result;
		}
	}
}
