#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

#include "brdf.hlsli"
#include "../rs/light_culling_rs.hlsli"

static float3 diffuseIBL(float3 kd, surface_info surface, TextureCube<float4> irradianceTexture, SamplerState clampSampler)
{
	float3 irradiance = irradianceTexture.SampleLevel(clampSampler, surface.N, 0).rgb;
	return kd * irradiance;
}

static float3 specularIBL(float3 F, surface_info surface, TextureCube<float4> environmentTexture, Texture2D<float2> brdf, SamplerState clampSampler, float specularScale = 1.f)
{
	uint width, height, numMipLevels;
	environmentTexture.GetDimensions(0, width, height, numMipLevels);
	float lod = surface.roughness * float(numMipLevels - 1);

	float3 prefilteredColor = environmentTexture.SampleLevel(clampSampler, surface.R, lod).rgb;
	float2 envBRDF = brdf.SampleLevel(clampSampler, float2(surface.roughness, surface.NdotV), 0);
	float3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y) * specularScale;

	return specular;
}

struct ambient_factors
{
	float3 kd;
	float3 ks;
};

static ambient_factors getAmbientFactors(surface_info surface)
{
	float3 F = fresnelSchlickRoughness(surface.NdotV, surface.F0, surface.roughness);
	float3 kd = float3(1.f, 1.f, 1.f) - F;
	kd *= 1.f - surface.metallic;

	ambient_factors result = { kd, F };
	return result;
}




static float sampleShadowMapSimple(float4x4 vp, float3 worldPosition,
	Texture2D<float> shadowMap, float4 viewport,
	SamplerComparisonState shadowMapSampler, float bias)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float2 lightUV = lightProjected.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	float visibility = 1.f;

	// This case is not handled by a border sampler because we are using a shadow map atlas.
	if (all(lightUV >= 0.f && lightUV <= 1.f))
	{
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
	float2 texelSize, float bias, float pcfRadius = 1.5f, float numPCFSamples = 16.f)
{
	float4 lightProjected = mul(vp, float4(worldPosition, 1.f));
	lightProjected.xyz /= lightProjected.w;

	float2 lightUV = lightProjected.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	float visibility = 1.f;

	// This case is not handled by a border sampler because we are using a shadow map atlas.
	if (all(lightUV >= 0.f && lightUV <= 1.f))
	{
		lightUV = lightUV * viewport.zw + viewport.xy;

		visibility = 0.f;
		for (float y = -pcfRadius; y <= pcfRadius + 0.01f; y += 1.f)
		{
			for (float x = -pcfRadius; x <= pcfRadius + 0.01f; x += 1.f)
			{
				visibility += shadowMap.SampleCmpLevelZero(
					shadowMapSampler,
					lightUV + float2(x, y) * texelSize,
					lightProjected.z - bias);
			}
		}
		visibility /= numPCFSamples;
	}

	return visibility;
}

static float samplePointLightShadowMapPCF(float3 worldPosition, float3 lightPosition,
	Texture2D<float> shadowMap,
	float4 viewport, float4 viewport2,
	SamplerComparisonState shadowMapSampler,
	float2 texelSize, float maxDistance, float pcfRadius = 1.5f, float numPCFSamples = 16.f)
{
	float3 L = worldPosition - lightPosition;
	float l = length(L);
	L /= l;

	float flip = L.z > 0.f ? 1.f : -1.f;
	float4 vp = L.z > 0.f ? viewport : viewport2;

	L.z *= flip;
	L.xy /= L.z + 1.f;

	float2 lightUV = L.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	lightUV = lightUV * vp.zw + vp.xy;

	float compareDistance = l / maxDistance;

	float bias = -0.001f * flip;

	float visibility = 0.f;
	for (float y = -pcfRadius; y <= pcfRadius + 0.01f; y += 1.f)
	{
		for (float x = -pcfRadius; x <= pcfRadius + 0.01f; x += 1.f)
		{
			visibility += shadowMap.SampleCmpLevelZero(
				shadowMapSampler,
				lightUV + float2(x, y) * texelSize,
				compareDistance - bias);
		}
	}
	visibility /= numPCFSamples;

	return visibility;
}

static float sampleCascadedShadowMapSimple(float4x4 vp[4], float3 worldPosition,
	Texture2D<float> shadowMap, float4 viewports[4],
	SamplerComparisonState shadowMapSampler,
	float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float4 blendDistances)
{
	if (numCascades == 0)
	{
		return 1.f;
	}
	else
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

		float nextCascadeVisibility = visibility;

		[branch]
		if (currentCascadeIndex != nextCascadeIndex && alpha != 0.f)
		{
			nextCascadeVisibility = sampleShadowMapSimple(vp[nextCascadeIndex], worldPosition,
				shadowMap, viewports[nextCascadeIndex],
				shadowMapSampler, bias[nextCascadeIndex]);
		}

		visibility = lerp(visibility, nextCascadeVisibility, alpha);
		return visibility;
	}
}

static float sampleCascadedShadowMapPCF(float4x4 vp[4], float3 worldPosition,
	Texture2D<float> shadowMap, float4 viewports[4],
	SamplerComparisonState shadowMapSampler,
	float2 texelSize, float pixelDepth, uint numCascades, float4 cascadeDistances, float4 bias, float4 blendDistances)
{
	if (numCascades == 0)
	{
		return 1.f;
	}
	else
	{
		float4 comparison = pixelDepth.xxxx > cascadeDistances;

		int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
		currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

		int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

		static const float pcfRadius[4] = {
			1.5f, 1.f, 0.5f, 0.f,
		};

		static const float numPCFSamples[4] = {
			16.f, 9.f, 4.f, 1.f,
		};

		float visibility = sampleShadowMapPCF(vp[currentCascadeIndex], worldPosition,
			shadowMap, viewports[currentCascadeIndex],
			shadowMapSampler, texelSize, bias[currentCascadeIndex], pcfRadius[currentCascadeIndex], numPCFSamples[currentCascadeIndex]);

		float blendEnd = cascadeDistances[currentCascadeIndex];
		float blendStart = blendEnd - blendDistances[currentCascadeIndex];
		float alpha = smoothstep(blendStart, blendEnd, pixelDepth);

		float nextCascadeVisibility = visibility;

		[branch]
		if (currentCascadeIndex != nextCascadeIndex && alpha != 0.f)
		{
			nextCascadeVisibility = sampleShadowMapPCF(vp[nextCascadeIndex], worldPosition,
				shadowMap, viewports[nextCascadeIndex],
				shadowMapSampler, texelSize, bias[nextCascadeIndex], pcfRadius[nextCascadeIndex], numPCFSamples[nextCascadeIndex]);
		}

		visibility = lerp(visibility, nextCascadeVisibility, alpha);
		return visibility;
	}
}






struct light_contribution
{
	float3 diffuse;
	float3 specular;

	void add(light_contribution other, float visibility = 1.f)
	{
		diffuse += other.diffuse * visibility;
		specular += other.specular * visibility;
	}

	float4 evaluate(float4 albedo)
	{
		float3 c = albedo.rgb * diffuse + specular;
		return float4(c, albedo.a);
	}

	void addSunLight(surface_info surface, lighting_cb lighting, float2 screenUV, float pixelDepth,
		Texture2D<float> shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapTexelSize,
		Texture2D<float> sssTexture, SamplerState clampSampler, float subSurfaceScale = 0.f);

	void addPointLights(surface_info surface, StructuredBuffer<point_light_cb> pointLights, StructuredBuffer<point_shadow_info> pointShadowInfos,
		StructuredBuffer<uint> tiledObjectsIndexList, uint2 tiledIndexData,
		Texture2D<float> shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapTexelSize);

	void addSpotLights(surface_info surface, StructuredBuffer<spot_light_cb> spotLights, StructuredBuffer<spot_shadow_info> spotShadowInfos,
		StructuredBuffer<uint> tiledObjectsIndexList, uint2 tiledIndexData,
		Texture2D<float> shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapTexelSize);

	void addImageBasedAmbientLighting(surface_info surface, TextureCube<float4> irradianceTexture, TextureCube<float4> prefilteredRadianceTexture,
		Texture2D<float2> brdf, Texture2D<float4> ssrTexture, Texture2D<float> aoTexture, SamplerState clampSampler, float2 screenUV, float globalIlluminationIntensity);

	void addRaytracedAmbientLighting(surface_info surface, lighting_cb lighting, Texture2D<float3> lightProbeIrradiance, Texture2D<float2> lightProbeDepth,
		Texture2D<float4> ssrTexture, Texture2D<float> aoTexture, SamplerState clampSampler, float2 screenUV);
};

static light_contribution calculateDirectLighting(surface_info surface, light_info light, float specularScale = 1.f)
{
	float D = distributionGGX(surface, light);
	float G = geometrySmith(surface, light);
	float3 F = fresnelSchlick(light.VdotH, surface.F0);

	float3 kD = float3(1.f, 1.f, 1.f) - F;
	kD *= 1.f - surface.metallic;
	float3 diffuse = kD * M_INV_PI * light.radiance * light.NdotL;

	float3 specular = (D * G * F) / max(4.f * surface.NdotV, 0.001f) * light.radiance * specularScale;

	light_contribution result = { diffuse, specular };
	return result;
}

static light_contribution calculateImageBasedAmbientLighting(surface_info surface, TextureCube<float4> irradianceTexture, TextureCube<float4> prefilteredRadianceTexture,
	Texture2D<float2> brdf, Texture2D<float4> ssrTexture, Texture2D<float> aoTexture, SamplerState clampSampler, float2 screenUV, float globalIlluminationIntensity)
{
	ambient_factors factors = getAmbientFactors(surface);

	float3 diffuse = diffuseIBL(factors.kd, surface, irradianceTexture, clampSampler);
	float3 specular = specularIBL(factors.ks, surface, prefilteredRadianceTexture, brdf, clampSampler);
	
	float4 ssr = ssrTexture.SampleLevel(clampSampler, screenUV, 0);
	specular = lerp(specular, ssr.rgb * surface.F, ssr.a);

	float ao = aoTexture.SampleLevel(clampSampler, screenUV, 0);
	float intensity = ao * globalIlluminationIntensity;

	light_contribution result = { diffuse * intensity, specular * intensity };
	return result;
}

static light_contribution calculateRaytracedAmbientLighting(surface_info surface, lighting_cb lighting, Texture2D<float3> lightProbeIrradiance, Texture2D<float2> lightProbeDepth,
	Texture2D<float4> ssrTexture, Texture2D<float> aoTexture, SamplerState clampSampler, float2 screenUV)
{
	float ao = aoTexture.SampleLevel(clampSampler, screenUV, 0);
	float intensity = ao * lighting.globalIlluminationIntensity;
	float4 ssr = ssrTexture.SampleLevel(clampSampler, screenUV, 0);

	light_contribution totalLighting;
	totalLighting.diffuse = lighting.lightProbeGrid.sampleIrradianceAtPosition(surface.P, surface.N, lightProbeIrradiance, lightProbeDepth, clampSampler) * intensity;
	totalLighting.specular = ssr.rgb * surface.F * intensity;
	return totalLighting;
}



void light_contribution::addSunLight(surface_info surface, lighting_cb lighting, float2 screenUV, float pixelDepth,
	Texture2D<float> shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapTexelSize,
	Texture2D<float> sssTexture, SamplerState clampSampler, float subSurfaceScale)
{
	float3 L = -lighting.sun.direction;

	light_info light;
	light.initialize(surface, L, lighting.sun.radiance);

	float visibility = sampleCascadedShadowMapPCF(lighting.sun.viewProjs, surface.P,
		shadowMap, lighting.sun.viewports,
		shadowSampler, lighting.shadowMapTexelSize, pixelDepth, lighting.sun.numShadowCascades,
		lighting.sun.cascadeDistances, lighting.sun.bias, lighting.sun.blendDistances);

	float sss = sssTexture.SampleLevel(clampSampler, screenUV, 0);
	visibility *= sss;

	[branch]
	if (visibility > 0.f)
	{
		add(calculateDirectLighting(surface, light), visibility);


		if (subSurfaceScale > 0.f)
		{
			// Subsurface scattering.
			// https://www.alanzucconi.com/2017/08/30/fast-subsurface-scattering-1/
			const float distortion = 0.4f;
			float3 sssH = L + surface.N * distortion;
			float sssVdotH = saturate(dot(surface.V, -sssH));

			float sssIntensity = sssVdotH * subSurfaceScale;
			diffuse += sssIntensity * lighting.sun.radiance * visibility;
		}

	}
}

void light_contribution::addPointLights(surface_info surface, StructuredBuffer<point_light_cb> pointLights, StructuredBuffer<point_shadow_info> pointShadowInfos,
	StructuredBuffer<uint> tiledObjectsIndexList, uint2 tiledIndexData,
	Texture2D<float> shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapTexelSize)
{
	const uint pointLightCount = (tiledIndexData.y >> 20) & 0x3FF;
	uint lightReadIndex = tiledIndexData.x + TILE_LIGHT_OFFSET;

	for (uint i = 0; i < pointLightCount; ++i)
	{
		point_light_cb pl = pointLights[tiledObjectsIndexList[lightReadIndex++]];

		light_info light;
		light.initializeFromPointLight(surface, pl);

		float visibility = 1.f;

		[branch]
		if (pl.shadowInfoIndex != -1)
		{
			point_shadow_info info = pointShadowInfos[pl.shadowInfoIndex];

			visibility = samplePointLightShadowMapPCF(surface.P, pl.position,
				shadowMap,
				info.viewport0, info.viewport1,
				shadowSampler,
				shadowMapTexelSize, pl.radius);
		}

		[branch]
		if (visibility > 0.f)
		{
			add(calculateDirectLighting(surface, light), visibility);
		}
	}
}

void light_contribution::addSpotLights(surface_info surface, StructuredBuffer<spot_light_cb> spotLights, StructuredBuffer<spot_shadow_info> spotShadowInfos,
	StructuredBuffer<uint> tiledObjectsIndexList, uint2 tiledIndexData, 
	Texture2D<float> shadowMap, SamplerComparisonState shadowSampler, float2 shadowMapTexelSize)
{
	const uint pointLightCount = (tiledIndexData.y >> 20) & 0x3FF;
	const uint spotLightCount = (tiledIndexData.y >> 10) & 0x3FF;
	uint lightReadIndex = tiledIndexData.x + TILE_LIGHT_OFFSET + pointLightCount;

	for (uint i = 0; i < spotLightCount; ++i)
	{
		spot_light_cb sl = spotLights[tiledObjectsIndexList[lightReadIndex++]];

		light_info light;
		light.initializeFromSpotLight(surface, sl);

		float visibility = 1.f;

		[branch]
		if (sl.shadowInfoIndex != -1)
		{
			spot_shadow_info info = spotShadowInfos[sl.shadowInfoIndex];
			visibility = sampleShadowMapPCF(info.viewProj, surface.P,
				shadowMap, info.viewport,
				shadowSampler,
				shadowMapTexelSize, info.bias);
		}

		[branch]
		if (visibility > 0.f)
		{
			add(calculateDirectLighting(surface, light), visibility);
		}
	}
}

void light_contribution::addImageBasedAmbientLighting(surface_info surface, TextureCube<float4> irradianceTexture, TextureCube<float4> prefilteredRadianceTexture,
	Texture2D<float2> brdf, Texture2D<float4> ssrTexture, Texture2D<float> aoTexture, SamplerState clampSampler, float2 screenUV, float globalIlluminationIntensity)
{
	add(calculateImageBasedAmbientLighting(surface, irradianceTexture, prefilteredRadianceTexture, brdf, ssrTexture, aoTexture, clampSampler, screenUV, globalIlluminationIntensity));
}

void light_contribution::addRaytracedAmbientLighting(surface_info surface, lighting_cb lighting, Texture2D<float3> lightProbeIrradiance, Texture2D<float2> lightProbeDepth,
	Texture2D<float4> ssrTexture, Texture2D<float> aoTexture, SamplerState clampSampler, float2 screenUV)
{
	add(calculateRaytracedAmbientLighting(surface, lighting, lightProbeIrradiance, lightProbeDepth, ssrTexture, aoTexture, clampSampler, screenUV));
}

#endif
