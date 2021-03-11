#include "depth_only_rs.hlsli"


ConstantBuffer<depth_only_transform_cb> transform	: register(b0);

struct mesh_position
{
	float3 position;
};

StructuredBuffer<mesh_position> prevFramePositions	: register(t0);

struct vs_input
{
	float3 position		: POSITION;
	uint vertexID       : SV_VertexID;
};

struct vs_output
{
	float3 ndc				: NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;

	float4 position			: SV_POSITION;
};

[RootSignature(ANIMATED_DEPTH_ONLY_RS)]
vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));
	OUT.ndc = OUT.position.xyw;

	float3 prevFramePosition = prevFramePositions[IN.vertexID].position;
	OUT.prevFrameNDC = mul(transform.prevFrameMVP, float4(prevFramePosition, 1.f)).xyw;
	return OUT;
}
