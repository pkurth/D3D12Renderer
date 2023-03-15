#include "terrain_height.hlsli"


ConstantBuffer<terrain_transform_cb> transform		: register(b0);
ConstantBuffer<terrain_cb> terrain					: register(b1);

Texture2D<float> heightmap							: register(t0);

struct vs_input
{
	uint vertexID	: SV_VertexID;
};

[RootSignature(TERRAIN_OUTLINE_RS)]
float4 main(vs_input IN) : SV_POSITION
{
	float3 position;
	float2 uv;
	terrainVertexPosition(terrain, IN.vertexID, heightmap, position, uv);

	return mul(transform.vp, float4(position, 1.f));
}
