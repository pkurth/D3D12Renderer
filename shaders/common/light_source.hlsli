#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

// Used for point and spot lights, because I dislike very high numbers.
#define LIGHT_RADIANCE_SCALE 1000.f

#define MAX_NUM_SUN_SHADOW_CASCADES 4

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
};

struct point_light_cb
{
	vec3 position;
	float radius; // Maximum distance.
	vec3 radiance;
	int shadowInfoIndex; // -1, if light casts no shadows.


	void initialize(vec3 position_, vec3 radiance_, float radius_, int shadowInfoIndex_ = -1)
	{
		position = position_;
		radiance = radiance_;
		radius = radius_;
		shadowInfoIndex = shadowInfoIndex_;
	}

#ifndef HLSL
	point_light_cb() {}

	point_light_cb(vec3 position_, vec3 radiance_, float radius_, int shadowInfoIndex_ = -1)
	{
		initialize(position_, radiance_, radius_, shadowInfoIndex_);
	}
#endif


};

struct spot_light_cb
{
	vec3 position;
	int innerAndOuterCutoff; // cos(innerAngle) << 16 | cos(outerAngle). Both are packed into 16 bit signed ints.
	vec3 direction;
	float maxDistance;
	vec3 radiance;
	int shadowInfoIndex; // -1, if light casts no shadows.


	void initialize(vec3 position_, vec3 direction_, vec3 radiance_, float innerAngle_, float outerAngle_, float maxDistance_, int shadowInfoIndex_ = -1)
	{
		position = position_;
		direction = direction_;
		radiance = radiance_;
		
		int inner = (int)(cos(innerAngle_) * ((1 << 15) - 1));
		int outer = (int)(cos(outerAngle_) * ((1 << 15) - 1));
		innerAndOuterCutoff = (inner << 16) | outer;

		maxDistance = maxDistance_;
		shadowInfoIndex = shadowInfoIndex_;
	}

#ifndef HLSL
	spot_light_cb() {}

	spot_light_cb(vec3 position_, vec3 direction_, vec3 radiance_, float innerAngle_, float outerAngle_, float maxDistance_, int shadowInfoIndex_ = -1)
	{
		initialize(position_, direction_, radiance_, innerAngle_, outerAngle_, maxDistance_, shadowInfoIndex_);
	}
#endif

	float getInnerCutoff()
#ifndef HLSL
		const
#endif
	{
		return (innerAndOuterCutoff >> 16) / float((1 << 15) - 1);
	}

	float getOuterCutoff()
#ifndef HLSL
		const
#endif
	{
		return (innerAndOuterCutoff & 0xFFFF) / float((1 << 15) - 1);
	}
};

struct spot_shadow_info
{
	mat4 viewProj;

	vec4 viewport;

	float bias;
	float padding0[3];
};

struct point_shadow_info
{
	vec4 viewport0;
	vec4 viewport1;
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

		for (float y = -1.5f; y <= 1.6f; y += 1.f)
		{
			for (float x = -1.5f; x <= 1.6f; x += 1.f)
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

static float samplePointLightShadowMapPCF(float3 worldPosition, float3 lightPosition,
	Texture2D<float> shadowMap, 
	vec4 viewport, vec4 viewport2, 
	SamplerComparisonState shadowMapSampler, 
	float2 texelSize, float maxDistance)
{
	float3 L = worldPosition - lightPosition;
	float l = length(L);
	L /= l;

	float flip = L.z > 0.f ? 1.f : -1.f;
	vec4 vp = L.z > 0.f ? viewport : viewport2;

	L.z *= flip;
	L.xy /= L.z + 1.f;

	float2 lightUV = L.xy * 0.5f + float2(0.5f, 0.5f);
	lightUV.y = 1.f - lightUV.y;

	lightUV = lightUV * vp.zw + vp.xy;

	float compareDistance = l / maxDistance;

	float bias = -0.001f * flip;

#if 0
	float visibility = shadowMap.SampleCmpLevelZero(
		shadowMapSampler,
		lightUV,
		compareDistance - bias);
#else
	float visibility = 0.f;
	for (float y = -1.5f; y <= 1.6f; y += 1.f)
	{
		for (float x = -1.5f; x <= 1.6f; x += 1.f)
		{
			visibility += shadowMap.SampleCmpLevelZero(
				shadowMapSampler,
				lightUV + float2(x, y) * texelSize,
				compareDistance - bias);
		}
	}
	visibility /= 16.f;
#endif

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
