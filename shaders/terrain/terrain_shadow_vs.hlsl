#include "depth_only_rs.hlsli"
#include "terrain_height.hlsli"


ConstantBuffer<shadow_transform_cb> transform	: register(b0);
ConstantBuffer<terrain_cb> terrain				: register(b1);

Texture2D<float> heightmap						: register(t0);

struct vs_input
{
	uint vertexID	: SV_VertexID;
};

struct vs_output
{
	float4 position			: SV_POSITION;
};

[RootSignature(TERRAIN_SHADOW_RS)]
vs_output main(vs_input IN)
{
	float3 position;
	float2 uv;
	terrainVertexPosition(terrain, IN.vertexID, heightmap, position, uv);

	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(position, 1.f));
	return OUT;
}
