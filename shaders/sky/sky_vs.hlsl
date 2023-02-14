#include "sky_rs.hlsli"

ConstantBuffer<sky_transform_cb> transform : register(b0);

struct vs_input
{
	uint vertexID	: SV_VertexID;
};

struct vs_output
{
	float3 uv				: TEXCOORDS;
	float3 ndc				: NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;

	float4 position			: SV_Position;
};

vs_output main(vs_input IN)
{
	vs_output OUT;

	uint b = 1u << IN.vertexID;
	float3 pos = float3(
		(0x287a & b) != 0, 
		(0x02af & b) != 0, 
		(0x31e3 & b) != 0
	) * 2.f - 1.f;

	OUT.uv = pos;
	OUT.position = mul(transform.vp, float4(pos, 1.f));
	OUT.position.z = OUT.position.w - 1e-6f;

	OUT.ndc = OUT.position.xyw;
	OUT.prevFrameNDC = mul(transform.prevFrameVP, float4(pos, 1.f)).xyw;

	return OUT;
}
