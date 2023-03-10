#include "tree_rs.hlsli"
#include "camera.hlsli"
#include "lighting.hlsli"
#include "normal.hlsli"
#include "material.hlsli"


struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;
	float3 treeDistances	: TREE_DISTANCES;

	float4 screenPosition	: SV_POSITION;
	bool isFrontFace		: SV_IsFrontFace;
};

struct ps_output
{
	float4 hdrColor				: SV_Target0;
	float4 worldNormalRoughness	: SV_Target1;
};

[earlydepthstencil]
[RootSignature(TREE_RS)]
ps_output main(ps_input IN)
{
	float3 N = IN.tbn[2];
	float roughness = 0.99f;

	// Output.
	ps_output OUT;

	OUT.hdrColor = float4(IN.treeDistances.zzz, 1.f);
	OUT.worldNormalRoughness = float4(packNormal(N), roughness, 0.f);

	return OUT;
}
