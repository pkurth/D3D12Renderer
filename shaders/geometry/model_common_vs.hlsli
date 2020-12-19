#include "model_rs.hlsli"


#if defined(DYNAMIC)
ConstantBuffer<dynamic_transform_cb> transform	: register(b0);
#elif defined(ANIMATED)
ConstantBuffer<dynamic_transform_cb> transform	: register(b0);
#else
ConstantBuffer<static_transform_cb> transform	: register(b0);
#endif

#ifdef ANIMATED
struct mesh_vertex
{
	float3 position;
	float2 uv;
	float3 normal;
	float3 tangent;
};

StructuredBuffer<mesh_vertex> prevFrameVertices : register(t0);
#endif

struct vs_input
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORDS;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;

#ifdef ANIMATED
	uint vertexID		: SV_VertexID;
#endif
};

struct vs_output
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;

#if defined(DYNAMIC) || defined(ANIMATED)
	float3 thisFrameNDC		: THIS_FRAME_NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;
#endif

	float4 position			: SV_POSITION;
};

vs_output main(vs_input IN)
{
	vs_output OUT;
	OUT.position = mul(transform.mvp, float4(IN.position, 1.f));

	OUT.uv = IN.uv;
	OUT.worldPosition = (mul(transform.m, float4(IN.position, 1.f))).xyz;

	float3 normal = normalize(mul(transform.m, float4(IN.normal, 0.f)).xyz);
	float3 tangent = normalize(mul(transform.m, float4(IN.tangent, 0.f)).xyz);
	float3 bitangent = normalize(cross(normal, tangent));
	OUT.tbn = float3x3(tangent, bitangent, normal);

#if defined(DYNAMIC)
	OUT.thisFrameNDC = OUT.position.xyw;
	OUT.prevFrameNDC = mul(transform.prevFrameMVP, float4(IN.position, 1.f)).xyw;
#elif defined(ANIMATED)
	OUT.thisFrameNDC = OUT.position.xyw;
	float3 prevFramePosition = prevFrameVertices[IN.vertexID].position;
	OUT.prevFrameNDC = mul(transform.prevFrameMVP, float4(prevFramePosition, 1.f)).xyw;
#endif

	return OUT;
}
