#include "cs.hlsli"
#include "random.hlsli"
#include "terrain_rs.hlsli"

ConstantBuffer<terrain_generation_cb> cb				: register(b0);
ConstantBuffer<terrain_generation_settings_cb> settings	: register(b1);

RWTexture2D<float> heights								: register(u0);
RWTexture2D<float2> normals								: register(u1);



[numthreads(16, 16, 1)]
[RootSignature(TERRAIN_GENERATION_RS)]
void main(cs_input IN)
{
	uint2 i = IN.dispatchThreadID.xy;

	if (i.x < settings.normalWidth && i.y < settings.normalHeight)
	{
		float2 position = float2(i) * settings.normalScale + cb.minCorner;

		float2 fbmPosition = position * settings.scale;
		float J_fbmPosition_position = settings.scale;

		float3 domainWarpValue = fbm(fbmPosition + settings.domainWarpNoiseOffset, settings.domainWarpOctaves);
		float domainWarpHeight = domainWarpValue.x;
		float2 J_domainWarpHeight_fbmPosition = domainWarpValue.yz;

		float2 warpedFbmPosition = fbmPosition + domainWarpHeight * settings.domainWarpStrength + settings.noiseOffset + 1000.f;
		float J_warpedFbmPosition_fbmPosition = 1.f;
		float J_warpedFbmPosition_domainWarpHeight = settings.domainWarpStrength;

		float3 value = fbm(warpedFbmPosition, settings.noiseOctaves);
		float height = value.x;
		float2 J_height_warpedFbmPosition = value.yz;

		float scaledHeight = height * 0.5f + 0.5f;
		float J_scaledHeight_height = 0.5f;

		float2 grad = J_scaledHeight_height * J_height_warpedFbmPosition *
			(J_warpedFbmPosition_fbmPosition + J_warpedFbmPosition_domainWarpHeight * J_domainWarpHeight_fbmPosition) * J_fbmPosition_position;

		normals[i] = -grad;
	}

	if (i.x < settings.heightWidth && i.y < settings.heightHeight)
	{
		float2 position = float2(i) * settings.positionScale + cb.minCorner;

		float2 fbmPosition = position * settings.scale;

		float3 domainWarpValue = fbm(fbmPosition + settings.domainWarpNoiseOffset, settings.domainWarpOctaves);

		float2 warpedFbmPosition = fbmPosition + domainWarpValue.x * settings.domainWarpStrength + settings.noiseOffset + 1000.f;
		float3 value = fbm(warpedFbmPosition); // We are using a lower number of noise octaves here, since the heightmap is lowres anyway.
		float height = value.x;

		height = height * 0.5f + 0.5f;

		heights[i] = height;
	}
}
