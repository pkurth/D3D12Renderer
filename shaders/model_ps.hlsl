#include "model_rs.hlsl"
#include "brdf.hlsl"
#include "camera.hlsl"
#include "light_culling_rs.hlsl"
#include "light_source.hlsl"

struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;
	float4 screenPosition	: SV_POSITION;
};

ConstantBuffer<pbr_material_cb> material	: register(b1);
ConstantBuffer<camera_cb> camera			: register(b2);


SamplerState wrapSampler				: register(s0);
SamplerState clampSampler				: register(s1);
SamplerComparisonState  shadowSampler	: register(s2);


Texture2D<float4> albedoTex				: register(t0);
Texture2D<float3> normalTex				: register(t1);
Texture2D<float> roughTex				: register(t2);
Texture2D<float> metalTex				: register(t3);

TextureCube<float4> irradianceTexture	: register(t0, space1);
TextureCube<float4> environmentTexture	: register(t1, space1);

Texture2D<float4> brdf					: register(t0, space2);

ConstantBuffer<directional_light_cb> sun		: register(b0, space3);
Texture2D<uint4> lightGrid						: register(t0, space3);
StructuredBuffer<uint> pointLightIndexList		: register(t1, space3);
StructuredBuffer<uint> spotLightIndexList		: register(t2, space3);
StructuredBuffer<point_light_cb> pointLights	: register(t3, space3);
StructuredBuffer<spot_light_cb> spotLights		: register(t4, space3);
Texture2D<float> sunShadowCascades[4]			: register(t5, space3);


[RootSignature(MODEL_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	uint flags = material.flags;

	float4 albedo = ((flags & USE_ALBEDO_TEXTURE)
		? albedoTex.Sample(wrapSampler, IN.uv)
		: float4(1.f, 1.f, 1.f, 1.f))
		* material.albedoTint;

	float3 N = (flags & USE_NORMAL_TEXTURE)
		? mul(normalTex.Sample(wrapSampler, IN.uv).xyz * 2.f - float3(1.f, 1.f, 1.f), IN.tbn)
		: IN.tbn[2];

	float roughness = (flags & USE_ROUGHNESS_TEXTURE)
		? roughTex.Sample(wrapSampler, IN.uv)
		: material.roughnessOverride;
	roughness = clamp(roughness, 0.01f, 0.99f);

	float metallic = (flags & USE_METALLIC_TEXTURE)
		? metalTex.Sample(wrapSampler, IN.uv)
		: material.metallicOverride;

	float ao = 1.f;// (flags & USE_AO_TEXTURE) ? RMAO.z : 1.f;


	float3 cameraPosition = camera.position.xyz;
	float3 camToP = IN.worldPosition - cameraPosition;
	float3 V = -normalize(camToP);
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo.xyz, metallic);

	float4 totalLighting = float4(0.f, 0.f, 0.f, albedo.w);

	// Point and spot lights.
	{
		const uint2 tileIndex = uint2(floor(IN.screenPosition.xy / LIGHT_CULLING_TILE_SIZE));
		const uint4 lightIndexData = lightGrid.Load(int3(tileIndex, 0));

		const uint pointLightOffset = lightIndexData.x;
		const uint pointLightCount = lightIndexData.y;

		const uint spotLightOffset = lightIndexData.z;
		const uint spotLightCount = lightIndexData.w;

		//return float4(spotLightCount, 0.f, 0.f, 1.f);


		// Point lights.
		for (uint lightIndex = pointLightOffset; lightIndex < pointLightOffset + pointLightCount; ++lightIndex)
		{
			uint index = pointLightIndexList[lightIndex];
			point_light_cb pl = pointLights[index];
			float distanceToLight = length(pl.position - IN.worldPosition);
			if (distanceToLight <= pl.radius)
			{
				float3 L = (pl.position - IN.worldPosition) / distanceToLight;
				float3 radiance = pl.radiance * getAttenuation(distanceToLight, pl.radius) * LIGHT_IRRADIANCE_SCALE;
				totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic);
			}
		}

		// Spot lights.
		for (lightIndex = spotLightOffset; lightIndex < spotLightOffset + spotLightCount; ++lightIndex)
		{
			uint index = spotLightIndexList[lightIndex];
			spot_light_cb sl = spotLights[index];
			float distanceToLight = length(sl.position - IN.worldPosition);

			if (distanceToLight <= sl.maxDistance)
			{
				float3 L = (sl.position - IN.worldPosition) / distanceToLight;

				float theta = dot(-L, sl.direction);
				if (theta > sl.outerCutoff)
				{
					float attenuation = getAttenuation(distanceToLight, sl.maxDistance);

					float epsilon = sl.innerCutoff - sl.outerCutoff;
					float intensity = saturate((theta - sl.outerCutoff) / epsilon);

					float totalIntensity = intensity * attenuation;
					if (totalIntensity > 0.f)
					{
						float3 radiance = sl.radiance * totalIntensity * LIGHT_IRRADIANCE_SCALE;
						totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic);
					}
				}
			}
		}
	}

	// Sun.
	{
		uint numCascades = sun.numShadowCascades;
		float4 cascadeDistances = sun.cascadeDistances;

		float currentPixelDepth = dot(camera.forward.xyz, camToP);
		float4 comparison = float4(currentPixelDepth, currentPixelDepth, currentPixelDepth, currentPixelDepth) > cascadeDistances;

		int currentCascadeIndex = dot(float4(numCascades > 0, numCascades > 1, numCascades > 2, numCascades > 3), comparison);
		currentCascadeIndex = min(currentCascadeIndex, numCascades - 1);

		int nextCascadeIndex = min(numCascades - 1, currentCascadeIndex + 1);

		float4 bias = sun.bias;
		float visibility = sampleShadowMap(sun.vp[currentCascadeIndex], IN.worldPosition, sunShadowCascades[currentCascadeIndex],
			shadowSampler, SUN_SHADOW_TEXEL_SIZE, bias[currentCascadeIndex]);

		// Blend between cascades.
		float currentPixelsBlendBandLocation = 1.f;
		if (numCascades > 1)
		{
			// Calculate blend amount.
			int blendIntervalBelowIndex = max(0, currentCascadeIndex - 1);
			float cascade0Factor = float(currentCascadeIndex > 0);
			float pixelDepth = currentPixelDepth - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;
			float blendInterval = cascadeDistances[currentCascadeIndex] - cascadeDistances[blendIntervalBelowIndex] * cascade0Factor;

			// Relative to current cascade. 0 means at nearplane of cascade, 1 at farplane of cascade.
			currentPixelsBlendBandLocation = 1.f - pixelDepth / blendInterval;
		}
		if (currentPixelsBlendBandLocation < sun.blendArea) // Blend area is relative!
		{
			float blendBetweenCascadesAmount = currentPixelsBlendBandLocation / sun.blendArea;
			float visibilityOfNextCascade = sampleShadowMap(sun.vp[nextCascadeIndex], IN.worldPosition, sunShadowCascades[nextCascadeIndex],
				shadowSampler, SUN_SHADOW_TEXEL_SIZE, bias[nextCascadeIndex]);
			visibility = lerp(visibilityOfNextCascade, visibility, blendBetweenCascadesAmount);
		}

		float3 radiance = sun.radiance * visibility; // No attenuation for sun.

		float3 L = -sun.direction.xyz;
		totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic);
	}

	// Ambient.
	totalLighting.xyz += calculateAmbientLighting(albedo.xyz, irradianceTexture, environmentTexture, brdf, clampSampler, N, V, F0, roughness, metallic, ao);

	return totalLighting;
}
