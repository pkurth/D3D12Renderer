#include "model_rs.hlsli"


#ifdef DYNAMIC
ConstantBuffer<dynamic_transform_cb> transform : register(b0);
#elif defined(ANIMATED)
ConstantBuffer<animated_transform_cb> transform : register(b0);
#else
ConstantBuffer<static_transform_cb> transform : register(b0);
#endif

struct vs_input
{
	float3 position		: POSITION;
	float2 uv			: TEXCOORDS;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;

#ifdef ANIMATED
	uint4 skinIndices	: SKIN_INDICES;
	float4 skinWeights	: SKIN_WEIGHTS;
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
#ifdef ANIMATED
	float4x4 s =
		transform.skinMatrices[IN.skinIndices.x] * IN.skinWeights.x +
		transform.skinMatrices[IN.skinIndices.y] * IN.skinWeights.y +
		transform.skinMatrices[IN.skinIndices.z] * IN.skinWeights.z +
		transform.skinMatrices[IN.skinIndices.w] * IN.skinWeights.w;

	float4x4 m = mul(transform.m, s);
	float4x4 mvp = mul(transform.mvp, s);
#else
	float4x4 m = transform.m;
	float4x4 mvp = transform.mvp;
#endif

	vs_output OUT;
	OUT.position = mul(mvp, float4(IN.position, 1.f));

	OUT.uv = IN.uv;
	OUT.worldPosition = (mul(m, float4(IN.position, 1.f))).xyz;

	float3 normal = normalize(mul(m, float4(IN.normal, 0.f)).xyz);
	float3 tangent = normalize(mul(m, float4(IN.tangent, 0.f)).xyz);
	float3 bitangent = normalize(cross(normal, tangent));
	OUT.tbn = float3x3(tangent, bitangent, normal);

#if defined(DYNAMIC)
	OUT.thisFrameNDC = OUT.position.xyw;
	OUT.prevFrameNDC = mul(transform.prevFrameMVP, float4(IN.position, 1.f)).xyw;
#elif defined(ANIMATED)
	// TODO
	OUT.thisFrameNDC = float3(0.f, 0.f, 1.f);
	OUT.prevFrameNDC = float3(0.f, 0.f, 1.f);
#endif

	return OUT;
}
