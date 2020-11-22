#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

// Used for point and spot lights, because I dislike very high numbers.
#define LIGHT_IRRADIANCE_SCALE 1000.f

#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SUN_SHADOW_DIMENSIONS 2048
#define SUN_SHADOW_TEXEL_SIZE (1.f / SUN_SHADOW_DIMENSIONS)

static float getAttenuation(float distance, float maxDistance)
{
	// https://imdoingitwrong.wordpress.com/2011/02/10/improved-light-attenuation/
	float relDist = min(distance / maxDistance, 1.f);
	float d = distance / (1.f - relDist * relDist);
	
	float att =  1.f / (d * d + 1.f);
	return att;
}

struct directional_light_cb
{
	mat4 vp[MAX_NUM_SUN_SHADOW_CASCADES];
	vec4 cascadeDistances;
	vec4 bias;

	vec3 direction;
	float blendArea;
	vec3 radiance;
	uint32 numShadowCascades;
};

struct point_light_cb
{
	vec3 position;
	float radius; // Maximum distance.
	vec3 radiance;
	uint32 padding;
};

struct spot_light_cb
{
	vec3 position;
	float innerCutoff; // cos(innerAngle).
	vec3 direction;
	float outerCutoff; // cos(outerAngle).
	vec3 radiance;
	float maxDistance;
};

#ifdef HLSL
static float sampleShadowMapSimple(float4x4 vp, float3 worldPosition, Texture2D<float> shadowMap, SamplerComparisonState shadowMapSampler, float bias)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float visibility = 1.f;

	if (lightProjected.z < 1.f)
	{
		float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
		lightUV.y = 1.f - lightUV.y;

		visibility = shadowMap.SampleCmpLevelZero(
			shadowMapSampler,
			lightUV,
			lightProjected.z - bias);
	}
	return visibility;
}

static float sampleShadowMapPCF(float4x4 vp, float3 worldPosition, Texture2D<float> shadowMap, SamplerComparisonState shadowMapSampler, 
	float texelSize, float bias)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float visibility = 1.f;

	if (lightProjected.z < 1.f)
	{
		visibility = 0.f;

		float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
		lightUV.y = 1.f - lightUV.y;

		for (float y = -1.5f; y <= 1.5f; y += 1.f)
		{
			for (float x = -1.5f; x <= 1.5f; x += 1.f)
			{
				visibility += shadowMap.SampleCmpLevelZero(
					shadowMapSampler,
					lightUV + float2(x, y) * texelSize,
					lightProjected.z - bias);
			}
		}
		visibility /= 16.f;
	}
	return visibility;
}

static float sampleCascadedShadowMapSimple(float4x4 vp[4], float3 worldPosition, Texture2D<float> shadowMaps[4], SamplerComparisonState shadowMapSampler,
	float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float blendArea)
{
	float4 comparison = pixelDepth.xxxx > cascadeDistances;

	int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
	currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

	int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

	float visibility = sampleShadowMapSimple(vp[currentCascadeIndex], worldPosition, shadowMaps[currentCascadeIndex],
		shadowMapSampler, bias[currentCascadeIndex]);

	// Blend between cascades.
	float currentPixelsBlendBandLocation = 1.f;
	if (numCascades > 1)
	{
		// Calculate blend amount.
		int blendIntervalBelowIndex = max(0, currentCascadeIndex - 1);
		float cascade0Factor = float(currentCascadeIndex > 0);
		float depth = pixelDepth - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;
		float blendInterval = cascadeDistances[currentCascadeIndex] - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;

		// Relative to current cascade. 0 means at nearplane of cascade, 1 at farplane of cascade.
		currentPixelsBlendBandLocation = 1.f - depth / blendInterval;
	}
	if (currentPixelsBlendBandLocation < blendArea) // Blend area is relative!
	{
		float blendBetweenCascadesAmount = currentPixelsBlendBandLocation / blendArea;
		float visibilityOfNextCascade = sampleShadowMapSimple(vp[nextCascadeIndex], worldPosition, shadowMaps[nextCascadeIndex],
			shadowMapSampler, bias[nextCascadeIndex]);
		visibility = lerp(visibilityOfNextCascade, visibility, blendBetweenCascadesAmount);
	}
	return visibility;
}

static float sampleCascadedShadowMapPCF(float4x4 vp[4], float3 worldPosition, Texture2D<float> shadowMaps[4], SamplerComparisonState shadowMapSampler, 
	float texelSize, float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float blendArea)
{
	float4 comparison = pixelDepth.xxxx > cascadeDistances;

	int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
	currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

	int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

	float visibility = sampleShadowMapPCF(vp[currentCascadeIndex], worldPosition, shadowMaps[currentCascadeIndex],
		shadowMapSampler, texelSize, bias[currentCascadeIndex]);

	// Blend between cascades.
	float currentPixelsBlendBandLocation = 1.f;
	if (numCascades > 1)
	{
		// Calculate blend amount.
		int blendIntervalBelowIndex = max(0, currentCascadeIndex - 1);
		float cascade0Factor = float(currentCascadeIndex > 0);
		float depth = pixelDepth - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;
		float blendInterval = cascadeDistances[currentCascadeIndex] - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;

		// Relative to current cascade. 0 means at nearplane of cascade, 1 at farplane of cascade.
		currentPixelsBlendBandLocation = 1.f - depth / blendInterval;
	}
	if (currentPixelsBlendBandLocation < blendArea) // Blend area is relative!
	{
		float blendBetweenCascadesAmount = currentPixelsBlendBandLocation / blendArea;
		float visibilityOfNextCascade = sampleShadowMapPCF(vp[nextCascadeIndex], worldPosition, shadowMaps[nextCascadeIndex],
			shadowMapSampler, texelSize, bias[nextCascadeIndex]);
		visibility = lerp(visibilityOfNextCascade, visibility, blendBetweenCascadesAmount);
	}
	return visibility;
}

#endif

#endif
