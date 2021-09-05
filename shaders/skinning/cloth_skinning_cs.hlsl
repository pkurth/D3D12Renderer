#include "cs.hlsli"
#include "skinning_rs.hlsli"
#include "math.hlsli"

#define BLOCK_SIZE 16
#define TILE_SIZE (BLOCK_SIZE + 2)


ConstantBuffer<cloth_skinning_cb> skinningCB		: register(b0);

StructuredBuffer<mesh_position> inputPositions		: register(t0);
RWStructuredBuffer<mesh_others> outputOthers		: register(u0);

groupshared float3 g_positions[TILE_SIZE * TILE_SIZE];
groupshared int3 g_normals[TILE_SIZE * TILE_SIZE];
groupshared int3 g_tangents[TILE_SIZE * TILE_SIZE];

[RootSignature(CLOTH_SKINNING_RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	const int2 firstVertexToProcess = IN.groupID.xy * BLOCK_SIZE;
	const int2 firstVertexToLoad = firstVertexToProcess - int2(1, 1);
	const int2 lastVertexToLoad = firstVertexToProcess + BLOCK_SIZE.xx;
	const int2 lastVertexToProcess = lastVertexToLoad - int2(1, 1);

	uint t;
	for (t = IN.groupIndex; t < TILE_SIZE * TILE_SIZE; t += BLOCK_SIZE * BLOCK_SIZE)
	{
		int2 index2D = firstVertexToLoad + unflatten2D(t, TILE_SIZE);
		index2D = clamp(index2D, 0, int2(skinningCB.gridSizeX, skinningCB.gridSizeY));
		const int index = flatten2D(index2D, skinningCB.gridSizeX);
		g_positions[t] = inputPositions[index].position;
		g_normals[t] = int3(0, 0, 0);
		g_tangents[t] = int3(0, 0, 0);
	}

	GroupMemoryBarrierWithGroupSync();

	float2 invSize = float2(1.f, 1.f) / float2(skinningCB.gridSizeX - 1, skinningCB.gridSizeY - 1);

	for (t = IN.groupIndex; t < (TILE_SIZE - 1) * (TILE_SIZE - 1); t += BLOCK_SIZE * BLOCK_SIZE)
	{
		const int tlIndex = t;
		const int trIndex = tlIndex + 1;
		const int blIndex = tlIndex + TILE_SIZE;
		const int brIndex = blIndex + 1;

		int a, b, c;
		if (t % 2 == 0)
		{
			a = tlIndex;
			b = blIndex;
			c = trIndex;
		}
		else
		{
			a = brIndex;
			b = trIndex;
			c = blIndex;
		}

		float3 edge0 = g_positions[b] - g_positions[a];
		float3 edge1 = g_positions[c] - g_positions[a];

		float3 normal = cross(edge0, edge1);
		int3 inormal = int3(normal * 0xFFFF);
		
		InterlockedAdd(g_normals[a].x, inormal.x);
		InterlockedAdd(g_normals[a].y, inormal.y);
		InterlockedAdd(g_normals[a].z, inormal.z);
		InterlockedAdd(g_normals[b].x, inormal.x);
		InterlockedAdd(g_normals[b].y, inormal.y);
		InterlockedAdd(g_normals[b].z, inormal.z);
		InterlockedAdd(g_normals[c].x, inormal.x);
		InterlockedAdd(g_normals[c].y, inormal.y);
		InterlockedAdd(g_normals[c].z, inormal.z);


		float2 aUV = (firstVertexToLoad + unflatten2D(a, TILE_SIZE)) * invSize;
		float2 bUV = (firstVertexToLoad + unflatten2D(b, TILE_SIZE)) * invSize;
		float2 cUV = (firstVertexToLoad + unflatten2D(c, TILE_SIZE)) * invSize;

		float2 deltaUV0 = cUV - aUV;
		float2 deltaUV1 = bUV - aUV;

		float f = 1.f / cross2(deltaUV0, deltaUV1);
		float3 tangent = f * (deltaUV1.y * edge0 - deltaUV0.y * edge1);
		int3 itangent = int3(tangent * 0xFFFF);

		InterlockedAdd(g_tangents[a].x, itangent.x);
		InterlockedAdd(g_tangents[a].y, itangent.y);
		InterlockedAdd(g_tangents[a].z, itangent.z);
		InterlockedAdd(g_tangents[b].x, itangent.x);
		InterlockedAdd(g_tangents[b].y, itangent.y);
		InterlockedAdd(g_tangents[b].z, itangent.z);
		InterlockedAdd(g_tangents[c].x, itangent.x);
		InterlockedAdd(g_tangents[c].y, itangent.y);
		InterlockedAdd(g_tangents[c].z, itangent.z);
	}

	GroupMemoryBarrierWithGroupSync();

	if (IN.groupIndex < BLOCK_SIZE * BLOCK_SIZE)
	{
		int2 blockIndex2D = unflatten2D(IN.groupIndex, BLOCK_SIZE);
		int2 globalIndex2D = blockIndex2D + firstVertexToProcess;
		if (all(globalIndex2D < int2(skinningCB.gridSizeX, skinningCB.gridSizeY)))
		{
			int globalIndex = flatten2D(globalIndex2D, skinningCB.gridSizeX);

			int t = IN.groupIndex + 1 + TILE_SIZE + blockIndex2D.y * TILE_SIZE;

			mesh_others others;
			others.uv = globalIndex2D * invSize;
			others.normal = (float)g_normals[t] / 0xFFFF;
			others.tangent = (float)g_tangents[t] / 0xFFFF;
			outputOthers[skinningCB.writeOffset + globalIndex] = others;
		}
	}
}
