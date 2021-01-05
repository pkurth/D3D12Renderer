#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

// Used for point and spot lights, because I dislike very high numbers.
#define LIGHT_IRRADIANCE_SCALE 1000.f

#define MAX_NUM_SUN_SHADOW_CASCADES 4
#define SUN_SHADOW_DIMENSIONS 2048

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
	vec4 viewports[MAX_NUM_SUN_SHADOW_CASCADES];

	vec4 cascadeDistances;
	vec4 bias;

	vec3 direction;
	uint32 numShadowCascades;
	vec3 radiance;
	uint32 padding;
	vec4 blendDistances;

	vec2 texelSize;
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
static float sampleShadowMapSimple(float4x4 vp, float3 worldPosition, 
	Texture2D<float> shadowMap, float4 viewport,
	SamplerComparisonState shadowMapSampler, float bias)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float visibility = 1.f;

	if (lightProjected.z < 1.f)
	{
		float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
		lightUV.y = 1.f - lightUV.y;

		lightUV = lightUV * viewport.zw + viewport.xy;

		visibility = shadowMap.SampleCmpLevelZero(
			shadowMapSampler,
			lightUV,
			lightProjected.z - bias);
	}
	return visibility;
}

static float sampleShadowMapPCF(float4x4 vp, float3 worldPosition, 
	Texture2D<float> shadowMap, float4 viewport,
	SamplerComparisonState shadowMapSampler, 
	float2 texelSize, float bias)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float visibility = 1.f;

	if (lightProjected.z < 1.f)
	{
		visibility = 0.f;

		float2 lightUV = lightProjected.xy * 0.5f + float2(0.5f, 0.5f);
		lightUV.y = 1.f - lightUV.y;

		lightUV = lightUV * viewport.zw + viewport.xy;

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

static float sampleCascadedShadowMapSimple(float4x4 vp[4], float3 worldPosition, 
	Texture2D<float> shadowMap, float4 viewports[4],
	SamplerComparisonState shadowMapSampler,
	float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float4 blendDistances)
{
	float blendArea = blendDistances.x;

	float4 comparison = pixelDepth.xxxx > cascadeDistances;

	int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
	currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

	int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

	float visibility = sampleShadowMapSimple(vp[currentCascadeIndex], worldPosition, 
		shadowMap, viewports[currentCascadeIndex],
		shadowMapSampler, bias[currentCascadeIndex]);

	float blendEnd = cascadeDistances[currentCascadeIndex];
	float blendStart = blendEnd - blendDistances[currentCascadeIndex];
	float alpha = smoothstep(blendStart, blendEnd, pixelDepth);

	float nextCascadeVisibility = (currentCascadeIndex == nextCascadeIndex || alpha == 0.f)
		? 1.f // No need to sample next cascade, if we are the last cascade or if we are not in the blend area.
		: sampleShadowMapSimple(vp[nextCascadeIndex], worldPosition, 
			shadowMap, viewports[nextCascadeIndex], 
			shadowMapSampler, bias[nextCascadeIndex]);

	visibility = lerp(visibility, nextCascadeVisibility, alpha);
	return visibility;
}

static float sampleCascadedShadowMapPCF(float4x4 vp[4], float3 worldPosition, 
	Texture2D<float> shadowMap, float4 viewports[4], 
	SamplerComparisonState shadowMapSampler, 
	float2 texelSize, float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float4 blendDistances)
{
	float4 comparison = pixelDepth.xxxx > cascadeDistances;

	int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
	currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

	int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

	float visibility = sampleShadowMapPCF(vp[currentCascadeIndex], worldPosition, 
		shadowMap, viewports[currentCascadeIndex],
		shadowMapSampler, texelSize, bias[currentCascadeIndex]);

	float blendEnd = cascadeDistances[currentCascadeIndex];
	float blendStart = blendEnd - blendDistances[currentCascadeIndex];
	float alpha = smoothstep(blendStart, blendEnd, pixelDepth);

	float nextCascadeVisibility = (currentCascadeIndex == nextCascadeIndex || alpha == 0.f) 
		? 1.f // No need to sample next cascade, if we are the last cascade or if we are not in the blend area.
		: sampleShadowMapPCF(vp[nextCascadeIndex], worldPosition, 
			shadowMap, viewports[nextCascadeIndex], 
			shadowMapSampler, texelSize, bias[nextCascadeIndex]);

	visibility = lerp(visibility, nextCascadeVisibility, alpha);
	return visibility;
}

#endif

#endif
