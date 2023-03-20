#include "depth_only_rs.hlsli"
#include "terrain_height.hlsli"


ConstantBuffer<depth_only_transform_cb> transform	: register(b0);
ConstantBuffer<terrain_cb> terrain					: register(b1);
ConstantBuffer<depth_only_object_id_cb> id			: register(b2);

Texture2D<float> heightmap							: register(t0);

struct vs_input
{
	uint vertexID	: SV_VertexID;
};

struct vs_output
{
	float3 ndc						: NDC;
	float3 prevFrameNDC				: PREV_FRAME_NDC;

	nointerpolation uint objectID	: OBJECT_ID;

	float4 position					: SV_POSITION;
};

[RootSignature(TERRAIN_DEPTH_ONLY_RS)]
vs_output main(vs_input IN)
{
	float3 position;
	float2 uv;
	terrainVertexPosition(terrain, IN.vertexID, heightmap, position, uv);

	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(position, 1.f));
	OUT.ndc = OUT.position.xyw;
	OUT.prevFrameNDC = mul(transform.prevFrameMVP, float4(position, 1.f)).xyw;
	OUT.objectID = id.id;
	return OUT;
}
