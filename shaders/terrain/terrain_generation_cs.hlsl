#include "cs.hlsli"
#include "random.hlsli"
#include "terrain_rs.hlsli"

ConstantBuffer<terrain_generation_cb> cb	: register(b0);

RWTexture2D<float> heights					: register(u0);
RWTexture2D<float2> normals					: register(u1);



[numthreads(16, 16, 1)]
[RootSignature(TERRAIN_GENERATION_RS)]
void main(cs_input IN)
{
	uint2 i = IN.dispatchThreadID.xy;

	if (i.x < cb.normalWidth && i.y < cb.normalHeight)
	{
		float2 position = float2(i) * cb.normalScale + cb.minCorner;

		float2 fbmPosition = position * 0.01f;
		float J_fbmPosition_position = 0.01f;

		float3 domainWarpValue = fbm(fbmPosition, 3);
		float domainWarpHeight = domainWarpValue.x;
		float2 J_domainWarpHeight_fbmPosition = domainWarpValue.yz;

		float2 warpedFbmPosition = fbmPosition + domainWarpHeight * cb.domainWarpStrength + 1000.f;
		float J_warpedFbmPosition_fbmPosition = 1.f;
		float J_warpedFbmPosition_domainWarpHeight = cb.domainWarpStrength;

		float3 value = fbm(warpedFbmPosition, 15);
		float height = value.x;
		float2 J_height_warpedFbmPosition = value.yz;

		float scaledHeight = height * 0.5f + 0.5f;
		float J_scaledHeight_height = 0.5f;

		float2 grad = J_scaledHeight_height * J_height_warpedFbmPosition *
			(J_warpedFbmPosition_fbmPosition + J_warpedFbmPosition_domainWarpHeight * J_domainWarpHeight_fbmPosition) * J_fbmPosition_position;


		float J_shaderHeight_absHeight = cb.amplitudeScale; // For the heights, this is done in the shader.
		grad *= J_shaderHeight_absHeight;

		normals[i] = -grad;
	}

	if (i.x < cb.heightWidth && i.y < cb.heightHeight)
	{
		float2 position = float2(i) * cb.positionScale + cb.minCorner;

		float2 fbmPosition = position * 0.01f;

		float3 domainWarpValue = fbm(fbmPosition, 3);

		float2 warpedFbmPosition = fbmPosition + domainWarpValue.x * cb.domainWarpStrength + 1000.f;
		float3 value = fbm(warpedFbmPosition);
		float height = value.x;

		height = height * 0.5f + 0.5f;

		heights[i] = height;
	}
}
