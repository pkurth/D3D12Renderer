#include "terrain_rs.hlsli"

ConstantBuffer<terrain_transform_cb> transform	: register(b0);
ConstantBuffer<terrain_cb> terrain				: register(b1);

Texture2D<float> heightmap						: register(t0);

struct vs_input
{
	uint vertexID	: SV_VertexID;
};

struct vs_output
{
	float2 uv				: TEXCOORDS;
	float3 worldPosition	: POSITION;
	float4 position			: SV_Position;
};

static uint getMask(uint isBorder, uint scaleDownByLOD)
{
	return ~0 << (scaleDownByLOD * isBorder);
}

#define ENSURE_CONSISTENT_ORIENTATION 1

vs_output main(vs_input IN)
{
	uint numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> terrain.lod;
	uint numVerticesPerDim = numSegmentsPerDim + 1;

	uint x = IN.vertexID % numVerticesPerDim;
	uint z = IN.vertexID / numVerticesPerDim;

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

	vs_output OUT;
	OUT.uv = uv;
	OUT.worldPosition = position;
	OUT.position = mul(transform.vp, float4(position, 1.f));
	return OUT;
}
