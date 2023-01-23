#ifndef TERRAIN_HEIGHT_HLSLI
#define TERRAIN_HEIGHT_HLSLI

#include "terrain_rs.hlsli"

static uint getMask(uint isBorder, uint scaleDownByLOD)
{
	return ~0 << (scaleDownByLOD * isBorder);
}

#define ENSURE_CONSISTENT_ORIENTATION 1

static void terrainVertexPosition(terrain_cb terrain, uint vertexID, Texture2D<float> heightmap, out float3 outPosition, out float2 outUV)
{
	uint numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> terrain.lod;
	uint numVerticesPerDim = numSegmentsPerDim + 1;

	uint x = vertexID % numVerticesPerDim;
	uint z = vertexID / numVerticesPerDim;

	uint scaleDownByLOD_left, scaleDownByLOD_right, scaleDownByLOD_top, scaleDownByLOD_bottom;
	DECODE_LOD_SCALE(terrain.scaleDownByLODs, scaleDownByLOD_left, scaleDownByLOD_right, scaleDownByLOD_top, scaleDownByLOD_bottom);

	const uint leftMask = getMask(x == 0, scaleDownByLOD_left);
	const uint rightMask = getMask(x == numVerticesPerDim - 1, scaleDownByLOD_right);
	const uint topMask = getMask(z == 0, scaleDownByLOD_top);
	const uint bottomMask = getMask(z == numVerticesPerDim - 1, scaleDownByLOD_bottom);

	const uint zMask = leftMask & rightMask;
	const uint xMask = topMask & bottomMask;

#if ENSURE_CONSISTENT_ORIENTATION
	// Will this vertex be shifted by the mask?
	const uint shiftedRight = (z & ~rightMask) != 0;
	const uint shiftedTop = (x & ~topMask) != 0;
#endif

	x &= xMask;
	z &= zMask;

#if ENSURE_CONSISTENT_ORIENTATION
	// Offset shifted vertices to next neighbor.
	x += shiftedTop * (1 << scaleDownByLOD_top);
	z += shiftedRight * (1 << scaleDownByLOD_right);
#endif



	float norm = 1.f / (float)numSegmentsPerDim;

	float height = heightmap[uint2(x << terrain.lod, z << terrain.lod)] * terrain.amplitudeScale;

	float2 uv = float2(x * norm, z * norm);

	float3 position = float3(uv.x * terrain.chunkSize, height, uv.y * terrain.chunkSize) + terrain.minCorner;

	outPosition = position;
	outUV = uv;
}



#endif

