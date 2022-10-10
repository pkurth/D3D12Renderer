#include "terrain_rs.hlsli"

ConstantBuffer<terrain_transform_cb> transform	: register(b0);
ConstantBuffer<terrain_cb> terrain				: register(b1);

struct vs_input
{
	uint vertexID	: SV_VertexID;
};

struct vs_output
{
	float2 uv		: TEXCOORDS;
	float4 position : SV_Position;
};

static uint getMask(uint isBorder, uint scaleDownByLOD)
{
	return ~(((1 << scaleDownByLOD) - 1) * isBorder);
}

vs_output main(vs_input IN)
{
	uint numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> terrain.lod;
	uint numVerticesPerDim = numSegmentsPerDim + 1;

	uint x = IN.vertexID % numVerticesPerDim;
	uint z = IN.vertexID / numVerticesPerDim;

	uint scaleDownByLOD_left, scaleDownByLOD_right, scaleDownByLOD_top, scaleDownByLOD_bottom;
	DECODE_LOD_SCALE(terrain.scaleDownByLODs, scaleDownByLOD_left, scaleDownByLOD_right, scaleDownByLOD_top, scaleDownByLOD_bottom);

	const uint scaleDownMask_z = getMask(x == 0, scaleDownByLOD_left) & getMask(x == numVerticesPerDim - 1, scaleDownByLOD_right);
	const uint scaleDownMask_x = getMask(z == 0, scaleDownByLOD_top) & getMask(z == numVerticesPerDim - 1, scaleDownByLOD_bottom);


	x &= scaleDownMask_x;
	z &= scaleDownMask_z;


	float norm = 1.f / (float)numSegmentsPerDim;

	float height = 0.f * terrain.amplitudeScale;

	float2 uv = float2(x * norm, z * norm);

	vs_output OUT;
	OUT.uv = uv;
	OUT.position = mul(transform.vp, float4(terrain.minCorner.x + uv.x * terrain.chunkSize, height, terrain.minCorner.y + uv.y * terrain.chunkSize, 1.f));
	return OUT;
}
