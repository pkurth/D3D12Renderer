#include "water_rs.hlsli"
#include "camera.hlsli"
#include "normal.hlsli"
#include "brdf.hlsli"
#include "lighting.hlsli"


SamplerState clampSampler								: register(s0);
SamplerState wrapSampler								: register(s1);
SamplerComparisonState shadowSampler					: register(s2);

ConstantBuffer<water_cb> cb								: register(b0, space1);
ConstantBuffer<camera_cb> camera						: register(b1, space1);
ConstantBuffer<lighting_cb> lighting					: register(b2, space1);

Texture2D<float3> opaqueColor							: register(t0);
Texture2D<float> opaqueDepth							: register(t1);
Texture2D<float3> normalmap1							: register(t2);
Texture2D<float3> normalmap2							: register(t3);
Texture2D<float3> foamTexture							: register(t4);
Texture2D<float> noiseTexture							: register(t5);

TextureCube<float4> irradianceTexture					: register(t0, space2);
TextureCube<float4> prefilteredRadianceTexture			: register(t1, space2);

Texture2D<float2> brdf									: register(t2, space2);

Texture2D<float> shadowMap								: register(t3, space2);

Texture2D<float> aoTexture								: register(t4, space2);
Texture2D<float> sssTexture								: register(t5, space2);
Texture2D<float4> ssrTexture							: register(t6, space2);



struct ps_input
{
	float3 worldPosition	: POSITION;

    float4 screenPosition	: SV_POSITION;
};


[earlydepthstencil]
[RootSignature(WATER_RS)]
float4 main(ps_input IN) : SV_TARGET
{
	float2 uv = IN.worldPosition.xz * 0.1f * cb.uvScale;
	float2 uv0 = uv - cb.uvOffset;
	float2 uv1 = (uv + cb.uvOffset) * 0.7f;

	float3 N0 = scaleNormalMap(sampleNormalMap(normalmap2, wrapSampler, uv0), cb.normalmapStrength);
	float3 N1 = scaleNormalMap(sampleNormalMap(normalmap1, wrapSampler, uv1), cb.normalmapStrength * 0.6f);

	float3 N = combineNormalMaps(N0, N1).xzy;


	const float refractionDistortionFactor = 0.015f;

	float2 screenUV = IN.screenPosition.xy * camera.invScreenDims;
	float2 distortedScreenUV = (screenUV + ((N.xz + N.xy) * 0.5f) * refractionDistortionFactor);


	float sceneDepth = camera.depthBufferDepthToEyeDepth(opaqueDepth.Sample(clampSampler, distortedScreenUV));
	float thisDepth = camera.depthBufferDepthToEyeDepth(IN.screenPosition.z);
	if (sceneDepth < thisDepth)
	{
		distortedScreenUV = screenUV;
		sceneDepth = camera.depthBufferDepthToEyeDepth(opaqueDepth.Sample(clampSampler, distortedScreenUV));
	}

	float3 sceneColor = opaqueColor.Sample(clampSampler, distortedScreenUV);

	float transition = saturate((sceneDepth - (thisDepth + cb.shallowDepth)) * cb.transitionStrength);


	const float noiseUVScale = 0.7f;
	float specularNoise = 1.f + 
		3.f * noiseTexture.Sample(wrapSampler, uv0 * noiseUVScale) * 
		noiseTexture.Sample(wrapSampler, uv1 * noiseUVScale) *
		noiseTexture.Sample(wrapSampler, uv * noiseUVScale);



	surface_info surface;

	surface.albedo = lerp(cb.shallowColor, cb.deepColor, transition);
	surface.N = N;
	surface.roughness = 0.2f;
	surface.roughness = clamp(surface.roughness, 0.01f, 0.99f);
	surface.metallic = 0.f;
	surface.emission = 0.f;

	surface.P = IN.worldPosition;
	float3 camToP = surface.P - camera.position.xyz;
	surface.V = -normalize(camToP);

	surface.inferRemainingProperties();



	float pixelDepth = dot(camera.forward.xyz, camToP);





	light_contribution totalLighting = { float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f) };


	// Sun.
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
			totalLighting.add(calculateDirectLighting(surface, light, specularNoise), visibility);
		}
	}


	// Ambient light.
	float ao = aoTexture.SampleLevel(clampSampler, screenUV, 0);

	ambient_factors factors = getAmbientFactors(surface);
	totalLighting.diffuse += diffuseIBL(factors.kd, surface, irradianceTexture, clampSampler) * lighting.globalIlluminationIntensity * ao;
	float3 specular = specularIBL(factors.ks, surface, prefilteredRadianceTexture, brdf, clampSampler, specularNoise);
	totalLighting.specular += specular * lighting.globalIlluminationIntensity * ao;


	float3 color = lerp(sceneColor, totalLighting.diffuse * surface.albedo.xyz, surface.albedo.a) + totalLighting.specular; // albedo

	return float4(color, 1.f);
}
