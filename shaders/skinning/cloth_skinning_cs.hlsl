#include "cs.hlsli"
#include "skinning_rs.hlsli"
#include "math.hlsli"


#define NUM_QUADS_PER_DIM 16
#define NUM_TOTAL_QUADS (NUM_QUADS_PER_DIM * NUM_QUADS_PER_DIM)
#define NUM_TOTAL_TRIS (NUM_TOTAL_QUADS * 2)
#define NUM_VERTS_TO_WRITE_PER_DIM (NUM_QUADS_PER_DIM - 1)
#define NUM_VERTS_TO_LOAD_PER_DIM (NUM_VERTS_TO_WRITE_PER_DIM + 2)
#define NUM_TOTAL_VERTS_TO_WRITE (NUM_VERTS_TO_WRITE_PER_DIM * NUM_VERTS_TO_WRITE_PER_DIM)
#define NUM_TOTAL_VERTS_TO_LOAD (NUM_VERTS_TO_LOAD_PER_DIM * NUM_VERTS_TO_LOAD_PER_DIM)

#define GROUP_FIRST_VERTEX_STRIDE NUM_VERTS_TO_WRITE_PER_DIM

#define BLOCK_SIZE NUM_QUADS_PER_DIM
#define NUM_THREADS (BLOCK_SIZE * BLOCK_SIZE)








ConstantBuffer<cloth_skinning_cb> skinningCB		: register(b0);

StructuredBuffer<mesh_position> inputPositions		: register(t0);
RWStructuredBuffer<mesh_position> outputPositions	: register(u0);
RWStructuredBuffer<mesh_others> outputOthers		: register(u1);

groupshared float3 g_positions[NUM_TOTAL_VERTS_TO_LOAD];
groupshared int3 g_normals[NUM_TOTAL_VERTS_TO_LOAD];
groupshared int3 g_tangents[NUM_TOTAL_VERTS_TO_LOAD];


static void processTriangle(int a, int b, int c, float2 aUV, float2 bUV, float2 cUV)
{
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


[RootSignature(CLOTH_SKINNING_RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	const int2 firstVertexToProcess = IN.groupID.xy * GROUP_FIRST_VERTEX_STRIDE;
	const int2 firstVertexToLoad = firstVertexToProcess - int2(1, 1);

	uint t;
	for (t = IN.groupIndex; t < NUM_TOTAL_VERTS_TO_LOAD; t += NUM_THREADS)
	{
		int2 globalIndex2D = firstVertexToLoad + unflatten2D(t, NUM_VERTS_TO_LOAD_PER_DIM);
		globalIndex2D = clamp(globalIndex2D, 0, int2(skinningCB.gridSizeX - 1, skinningCB.gridSizeY - 1));
		const int index = flatten2D(globalIndex2D, skinningCB.gridSizeX);
		g_positions[t] = inputPositions[index].position;
		g_normals[t] = int3(0, 0, 0);
		g_tangents[t] = int3(0, 0, 0);
	}

	GroupMemoryBarrierWithGroupSync();

	// Each thread processes one quad -> two triangles.
	{
		const int2 quadIndex = unflatten2D(IN.groupIndex, NUM_QUADS_PER_DIM);
		const int tlIndex = flatten2D(quadIndex, NUM_VERTS_TO_LOAD_PER_DIM);
		const int trIndex = tlIndex + 1;
		const int blIndex = tlIndex + NUM_VERTS_TO_LOAD_PER_DIM;
		const int brIndex = blIndex + 1;

		const float2 tlUV = float2(0.f, 0.f);
		const float2 trUV = float2(1.f, 0.f);
		const float2 blUV = float2(0.f, 1.f);
		const float2 brUV = float2(1.f, 1.f);

		processTriangle(tlIndex, blIndex, trIndex, tlUV, blUV, trUV);
		processTriangle(brIndex, trIndex, blIndex, brUV, trUV, blUV);
	}

	GroupMemoryBarrierWithGroupSync();

	float2 invSize = float2(1.f, 1.f) / float2(skinningCB.gridSizeX - 1, skinningCB.gridSizeY - 1);

	for (t = IN.groupIndex; t < NUM_TOTAL_VERTS_TO_WRITE; t += NUM_THREADS)
	{
		uint2 blockIndex2D = unflatten2D(t, NUM_VERTS_TO_WRITE_PER_DIM);
		uint2 globalIndex2D = blockIndex2D + firstVertexToProcess;

		if (globalIndex2D.x < skinningCB.gridSizeX && globalIndex2D.y < skinningCB.gridSizeY)
		{
			uint globalIndex = flatten2D(globalIndex2D, skinningCB.gridSizeX);
			uint readIndex = (blockIndex2D.y + 1) * NUM_VERTS_TO_LOAD_PER_DIM + blockIndex2D.x + 1;

			mesh_position position;
			position.position = g_positions[readIndex];
			outputPositions[skinningCB.writeOffset + globalIndex] = position;

			mesh_others others;
			others.uv = globalIndex2D * invSize;
			others.normal = (float3)g_normals[readIndex] / 0xFFFF;
			others.tangent = (float3)g_tangents[readIndex] / 0xFFFF;
			outputOthers[skinningCB.writeOffset + globalIndex] = others;
		}
	}
}
