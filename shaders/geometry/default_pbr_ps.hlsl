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

ConstantBuffer<pbr_material_cb> material				: register(b0, space1);
ConstantBuffer<camera_cb> camera						: register(b1, space1);
ConstantBuffer<lighting_cb> lighting					: register(b2, space1);


SamplerState wrapSampler								: register(s0);
SamplerState clampSampler								: register(s1);
SamplerComparisonState shadowSampler					: register(s2);


Texture2D<float4> albedoTex								: register(t0, space1);
Texture2D<float3> normalTex								: register(t1, space1);
Texture2D<float> roughTex								: register(t2, space1);
Texture2D<float> metalTex								: register(t3, space1);


ConstantBuffer<directional_light_cb> sun				: register(b0, space2);

TextureCube<float4> irradianceTexture					: register(t0, space2);
TextureCube<float4> environmentTexture					: register(t1, space2);

Texture2D<float4> brdf									: register(t2, space2);

Texture2D<uint2> tiledCullingGrid						: register(t3, space2);
StructuredBuffer<uint> tiledObjectsIndexList			: register(t4, space2);
StructuredBuffer<point_light_cb> pointLights			: register(t5, space2);
StructuredBuffer<spot_light_cb> spotLights				: register(t6, space2);
StructuredBuffer<decal_cb> decals						: register(t7, space2);
Texture2D<float> shadowMap								: register(t8, space2);
StructuredBuffer<point_shadow_info> pointShadowInfos	: register(t9, space2);
StructuredBuffer<spot_shadow_info> spotShadowInfos		: register(t10, space2);

struct ps_output
{
	float4 hdrColor		: SV_Target0;
	float2 worldNormal	: SV_Target1;
};

[RootSignature(DEFAULT_PBR_RS)]
ps_output main(ps_input IN)
{
	uint flags = material.flags;

	surface_info surface;

	surface.albedo = ((flags & USE_ALBEDO_TEXTURE)
		? albedoTex.Sample(wrapSampler, IN.uv)
		: float4(1.f, 1.f, 1.f, 1.f))
		* unpackColor(material.albedoTint);

	surface.N = (flags & USE_NORMAL_TEXTURE)
		? mul(normalTex.Sample(wrapSampler, IN.uv).xyz * 2.f - float3(1.f, 1.f, 1.f), IN.tbn)
		: IN.tbn[2];
	surface.N = normalize(surface.N);

	surface.roughness = (flags & USE_ROUGHNESS_TEXTURE)
		? roughTex.Sample(wrapSampler, IN.uv)
		: getRoughnessOverride(material);
	surface.roughness = clamp(surface.roughness, 0.01f, 0.99f);

	surface.metallic = (flags & USE_METALLIC_TEXTURE)
		? metalTex.Sample(wrapSampler, IN.uv)
		: getMetallicOverride(material);

	surface.emission = material.emission;

	surface.P = IN.worldPosition;
	float3 camToP = surface.P - camera.position.xyz;
	surface.V = -normalize(camToP);

	float pixelDepth = dot(camera.forward.xyz, camToP);


	surface.inferRemainingProperties();




	float4 totalLighting = float4(surface.emission, surface.albedo.w);


	// Tiled lighting.
	{
		const uint2 tileIndex = uint2(IN.screenPosition.xy / LIGHT_CULLING_TILE_SIZE);
		const uint2 tiledIndexData = tiledCullingGrid[tileIndex];

		const uint pointLightCount = (tiledIndexData.y >> 20) & 0x3FF;
		const uint spotLightCount = (tiledIndexData.y >> 10) & 0x3FF;
		const uint decalCount = (tiledIndexData.y >> 0) & 0x3FF;

		uint readIndex = tiledIndexData.x; // Offset.

		float4 decalAlbedoAccum = (float4)0.f;

		// Decals.
		for (uint i = 0; i < decalCount; ++i)
		{
			decal_cb decal = decals[tiledObjectsIndexList[readIndex++]];

			float3 offset = surface.P - decal.position;
			float3 local = float3(
				dot(decal.right, offset) / (dot(decal.right, decal.right)),
				dot(decal.up, offset) / (dot(decal.up, decal.up)),
				dot(decal.forward, offset) / (dot(decal.forward, decal.forward))
			);

			if (all(local >= -1.f && local <= 1.f) && dot(surface.N, decal.forward) <= 0.f)
			{
				float2 uv = local.xy * 0.5f + 0.5f;
				float4 decalAlbedo = unpackColor(decal.albedoTint); // TODO: Sample texture.

				surface.albedo = decalAlbedo;

				/*decalAlbedoAccum.rgb = (1 - decalAlbedoAccum.a) * (decalAlbedo.a * decalAlbedo.rgb) + decalAlbedoAccum.rgb;
				decalAlbedoAccum.a = decalAlbedo.a + (1 - decalAlbedo.a) * decalAccumulation.a;
				[branch]
				if (decalAlbedoAccum.a >= 1.f)
				{
					break;
				}*/

			}
		}

		// TODO: Infer remaining surface properties here.

		// Point lights.
		for (i = 0; i < pointLightCount; ++i)
		{
			point_light_cb pl = pointLights[tiledObjectsIndexList[readIndex++]];

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
					lighting.shadowMapTexelSize, pl.radius);
			}

			[branch]
			if (visibility > 0.f)
			{
				totalLighting.xyz += calculateDirectLighting(surface, light) * visibility;
			}
		}

		// Spot lights.
		for (i = 0; i < spotLightCount; ++i)
		{
			spot_light_cb sl = spotLights[tiledObjectsIndexList[readIndex++]];

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
					lighting.shadowMapTexelSize, info.bias);
			}

			[branch]
			if (visibility > 0.f)
			{
				totalLighting.xyz += calculateDirectLighting(surface, light) * visibility;
			}
		}
	}

	// Sun.
	{
		float3 L = -normalize(sun.direction);

		light_info light;
		light.initialize(surface, L, sun.radiance);

		float visibility = sampleCascadedShadowMapPCF(sun.vp, surface.P, 
			shadowMap, sun.viewports,
			shadowSampler, lighting.shadowMapTexelSize, pixelDepth, sun.numShadowCascades,
			sun.cascadeDistances, sun.bias, sun.blendDistances);

		[branch]
		if (visibility > 0.f)
		{
			totalLighting.xyz += calculateDirectLighting(surface, light) * visibility;
		}
	}

#if 1
	// Ambient.
	totalLighting.xyz += calculateAmbientLighting(surface, irradianceTexture, environmentTexture, brdf, clampSampler) * lighting.environmentIntensity;
#endif

	ps_output OUT;
	OUT.hdrColor = totalLighting;
	OUT.worldNormal = packNormal(surface.N);

	return OUT;
}
