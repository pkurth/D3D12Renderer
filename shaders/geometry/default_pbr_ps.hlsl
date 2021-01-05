#include "default_pbr_rs.hlsli"
#include "brdf.hlsli"
#include "camera.hlsli"
#include "light_culling_rs.hlsli"
#include "light_source.hlsli"
#include "normal.hlsli"
#include "material.hlsli"

struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3x3 tbn			: TANGENT_FRAME;
	float3 worldPosition	: POSITION;

	float4 screenPosition	: SV_POSITION;
};

ConstantBuffer<pbr_material_cb> material		: register(b0, space1);
ConstantBuffer<camera_cb> camera				: register(b1, space1);
ConstantBuffer<lighting_cb> lighting			: register(b2, space1);


SamplerState wrapSampler						: register(s0);
SamplerState clampSampler						: register(s1);
SamplerComparisonState shadowSampler			: register(s2);


Texture2D<float4> albedoTex						: register(t0, space1);
Texture2D<float3> normalTex						: register(t1, space1);
Texture2D<float> roughTex						: register(t2, space1);
Texture2D<float> metalTex						: register(t3, space1);


ConstantBuffer<directional_light_cb> sun		: register(b0, space2);

TextureCube<float4> irradianceTexture			: register(t0, space2);
TextureCube<float4> environmentTexture			: register(t1, space2);

Texture2D<float4> brdf							: register(t2, space2);

Texture2D<uint4> lightGrid						: register(t3, space2);
StructuredBuffer<uint> pointLightIndexList		: register(t4, space2);
StructuredBuffer<uint> spotLightIndexList		: register(t5, space2);
StructuredBuffer<point_light_cb> pointLights	: register(t6, space2);
StructuredBuffer<spot_light_cb> spotLights		: register(t7, space2);
Texture2D<float> sunShadowCascades[4]			: register(t8, space2);
Texture2D<float4> volumetrics					: register(t12, space2);

struct ps_output
{
	float4 hdrColor		: SV_Target0;
	float2 worldNormal	: SV_Target1;
};

[RootSignature(DEFAULT_PBR_RS)]
ps_output main(ps_input IN)
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
		: getRoughnessOverride(material);
	roughness = clamp(roughness, 0.01f, 0.99f);

	float metallic = (flags & USE_METALLIC_TEXTURE)
		? metalTex.Sample(wrapSampler, IN.uv)
		: getMetallicOverride(material);

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
		float pixelDepth = dot(camera.forward.xyz, camToP);
		float visibility = sampleCascadedShadowMapPCF(sun.vp, IN.worldPosition, sunShadowCascades, shadowSampler, SUN_SHADOW_TEXEL_SIZE, pixelDepth, sun.numShadowCascades,
			sun.cascadeDistances, sun.bias, sun.blendDistances);

		float3 radiance = sun.radiance * visibility; // No attenuation for sun.

		float3 L = -sun.direction;
		totalLighting.xyz += calculateDirectLighting(albedo.xyz, radiance, N, L, V, F0, roughness, metallic);
	}

	// Ambient.
	totalLighting.xyz += calculateAmbientLighting(albedo.xyz, irradianceTexture, environmentTexture, brdf, clampSampler, N, V, F0, roughness, metallic, ao) * lighting.environmentIntensity;

	ps_output OUT;
	OUT.hdrColor = totalLighting;
	OUT.worldNormal = packNormal(N);

	return OUT;
}