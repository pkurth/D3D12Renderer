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

Texture2D<float2> brdf									: register(t2, space2);

Texture2D<uint4> tiledCullingGrid						: register(t3, space2);
StructuredBuffer<uint> tiledObjectsIndexList			: register(t4, space2);
StructuredBuffer<point_light_cb> pointLights			: register(t5, space2);
StructuredBuffer<spot_light_cb> spotLights				: register(t6, space2);
StructuredBuffer<pbr_decal_cb> decals					: register(t7, space2);
Texture2D<float> shadowMap								: register(t8, space2);
StructuredBuffer<point_shadow_info> pointShadowInfos	: register(t9, space2);
StructuredBuffer<spot_shadow_info> spotShadowInfos		: register(t10, space2);

Texture2D<float4> decalTextureAtlas                     : register(t11, space2);


struct ps_output
{
	float4 hdrColor		: SV_Target0;

#ifndef TRANSPARENT
	float2 worldNormal	: SV_Target1;
	float4 reflectance	: SV_Target2;
#endif
};

[RootSignature(DEFAULT_PBR_RS)]
ps_output main(ps_input IN)
{
	uint flags = material.getFlags();

	surface_info surface;

	surface.albedo = ((flags & USE_ALBEDO_TEXTURE)
		? albedoTex.Sample(wrapSampler, IN.uv)
		: float4(1.f, 1.f, 1.f, 1.f))
		* material.getAlbedo();

	const float normalMapStrength = material.getNormalMapStrength() * 0.2f;
	surface.N = (flags & USE_NORMAL_TEXTURE)
		? mul(float3(normalMapStrength, normalMapStrength, 1.f) * (normalTex.Sample(wrapSampler, IN.uv).xyz * 2.f - 1.f), IN.tbn)
		: IN.tbn[2];
	surface.N = normalize(surface.N);

	surface.roughness = (flags & USE_ROUGHNESS_TEXTURE)
		? roughTex.Sample(wrapSampler, IN.uv)
		: material.getRoughnessOverride();
	surface.roughness = clamp(surface.roughness, 0.01f, 0.99f);

	surface.metallic = (flags & USE_METALLIC_TEXTURE)
		? metalTex.Sample(wrapSampler, IN.uv)
		: material.getMetallicOverride();

	surface.emission = material.emission;

	surface.P = IN.worldPosition;
	float3 camToP = surface.P - camera.position.xyz;
	surface.V = -normalize(camToP);

	float pixelDepth = dot(camera.forward.xyz, camToP);




	light_contribution totalLighting = { float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f) };


	// Tiled lighting.
	const uint2 tileIndex = uint2(IN.screenPosition.xy / LIGHT_CULLING_TILE_SIZE);

#ifndef TRANSPARENT
	const uint2 tiledIndexData = tiledCullingGrid[tileIndex].xy;
#else
	const uint2 tiledIndexData = tiledCullingGrid[tileIndex].zw;
#endif

	const uint pointLightCount = (tiledIndexData.y >> 20) & 0x3FF;
	const uint spotLightCount = (tiledIndexData.y >> 10) & 0x3FF;
	const uint decalReadOffset = tiledIndexData.x;
	uint lightReadIndex = tiledIndexData.x + TILE_LIGHT_OFFSET;



	// Decals.
#if 1
	float3 decalAlbedoAccum = (float3)0.f;
	float decalRoughnessAccum = 0.f;
	float decalMetallicAccum = 0.f;
	float decalAlphaAccum = 0.f;

	for (uint decalBucketIndex = 0; (decalBucketIndex < NUM_DECAL_BUCKETS) && (decalAlphaAccum < 1.f); ++decalBucketIndex)
	{
		uint bucket = tiledObjectsIndexList[decalReadOffset + decalBucketIndex];

		[loop]
		while (bucket)
		{
			const uint indexOfLowestSetBit = firstbitlow(bucket);
			bucket ^= 1 << indexOfLowestSetBit; // Unset this bit.

			uint decalIndex = decalBucketIndex * 32 + indexOfLowestSetBit;
			decalIndex = MAX_NUM_TOTAL_DECALS - decalIndex - 1; // Reverse of operation in culling shader.
			pbr_decal_cb decal = decals[decalIndex];

			float3 offset = surface.P - decal.position;
			float3 local = float3(
				dot(decal.right, offset) / (dot(decal.right, decal.right)),
				dot(decal.up, offset) / (dot(decal.up, decal.up)),
				dot(decal.forward, offset) / (dot(decal.forward, decal.forward))
				);

			float decalStrength = saturate(dot(-surface.N, normalize(decal.forward)));

			[branch]
			if (all(local >= -1.f && local <= 1.f) && decalStrength > 0.f)
			{
				float2 uv = local.xy * 0.5f + 0.5f;                
				
				float4 viewport = decal.getViewport();
				uv = viewport.xy + uv * viewport.zw;

				// Since this loop has variable length, we cannot use automatic mip-selection here. Gradients may be undefined.
				const float4 decalAlbedo = decalTextureAtlas.SampleLevel(wrapSampler, uv, 0) * decal.getAlbedo();
				const float decalRoughness = decal.getRoughnessOverride();
				const float decalMetallic = decal.getMetallicOverride();
				
				const float alpha = decalAlbedo.a * decalStrength;
				const float oneMinusDecalAlphaAccum = 1.f - decalAlphaAccum;

				decalAlbedoAccum += oneMinusDecalAlphaAccum * (alpha * decalAlbedo.rgb);
				decalRoughnessAccum += oneMinusDecalAlphaAccum * (alpha * decalRoughness);
				decalMetallicAccum += oneMinusDecalAlphaAccum * (alpha * decalMetallic);

				decalAlphaAccum = alpha + (1.f - alpha) * decalAlphaAccum;

				[branch]
				if (decalAlphaAccum >= 1.f)
				{
					decalAlphaAccum = 1.f;
					break;
				}
			}
		}
	}

	surface.albedo.rgb = lerp(surface.albedo.rgb, decalAlbedoAccum, decalAlphaAccum);
	surface.roughness = lerp(surface.roughness, decalRoughnessAccum, decalAlphaAccum);
	surface.metallic = lerp(surface.metallic, decalMetallicAccum, decalAlphaAccum);
#endif

	surface.inferRemainingProperties();

	uint i;

	// Point lights.
	for (i = 0; i < pointLightCount; ++i)
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
				lighting.shadowMapTexelSize, pl.radius);
		}

		[branch]
		if (visibility > 0.f)
		{
			totalLighting.add(calculateDirectLighting(surface, light), visibility);
		}
	}

	// Spot lights.
	for (i = 0; i < spotLightCount; ++i)
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
				lighting.shadowMapTexelSize, info.bias);
		}

		[branch]
		if (visibility > 0.f)
		{
			totalLighting.add(calculateDirectLighting(surface, light), visibility);
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
			totalLighting.add(calculateDirectLighting(surface, light), visibility);
		}
	}


	// Ambient light.
	ambient_factors factors = getAmbientFactors(surface);
	totalLighting.diffuse += diffuseIBL(factors.kd, surface, irradianceTexture, clampSampler) * lighting.environmentIntensity;

#ifdef TRANSPARENT
	// Only add ambient specular for transparent objects. Opaque objects get their ambient specular in a later render pass.
	totalLighting.specular += specularIBL(factors.ks, surface, environmentTexture, brdf, clampSampler) * lighting.environmentIntensity;
#endif



	// Output.
	ps_output OUT;

#ifndef TRANSPARENT
	OUT.hdrColor = totalLighting.evaluate(surface.albedo);
	OUT.hdrColor.rgb += surface.emission;

	OUT.worldNormal = packNormal(surface.N);
	OUT.reflectance = float4(factors.ks, surface.roughness);
#else

	OUT.hdrColor = mergeAlphaBlended(totalLighting.diffuse * surface.albedo.rgb, totalLighting.specular, surface.emission, surface.albedo.a);

	// Normal and reflectance are not needed for transparent surfaces.
#endif

	return OUT;
}
