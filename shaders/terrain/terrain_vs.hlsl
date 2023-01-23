#include "terrain_rs.hlsli"
#include "terrain_height.hlsli"

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

vs_output main(vs_input IN)
{
	float3 position;
	float2 uv;
	terrainVertexPosition(terrain, IN.vertexID, heightmap, position, uv);

	vs_output OUT;
	OUT.uv = uv;
	OUT.worldPosition = position;
	OUT.position = mul(transform.vp, float4(position, 1.f));
	return OUT;
}
