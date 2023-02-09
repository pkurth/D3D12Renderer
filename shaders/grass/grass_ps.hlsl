#include "grass_rs.hlsli"
#include "camera.hlsli"
#include "lighting.hlsli"
#include "normal.hlsli"
#include "brdf.hlsli"

ConstantBuffer<camera_cb> camera		: register(b1);
ConstantBuffer<lighting_cb> lighting	: register(b2);

TextureCube<float4> irradianceTexture			: register(t0, space2);
TextureCube<float4> prefilteredRadianceTexture	: register(t1, space2);

Texture2D<float2> brdf							: register(t2, space2);

Texture2D<float> shadowMap						: register(t3, space2);

Texture2D<float> aoTexture						: register(t4, space2);
Texture2D<float> sssTexture						: register(t5, space2);
Texture2D<float4> ssrTexture					: register(t6, space2);

SamplerState clampSampler						: register(s0);
SamplerComparisonState shadowSampler			: register(s1);



struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPosition	: POSITION;

	float4 screenPosition	: SV_POSITION;
	bool isFrontFace		: SV_IsFrontFace;
};


struct ps_output
{
	float4 hdrColor				: SV_Target0;
	float4 worldNormalRoughness	: SV_Target1;
};

[earlydepthstencil]
[RootSignature(GRASS_RS)]
ps_output main(ps_input IN)
{
	float3 L = -lighting.sun.direction;

	float3 N = IN.normal;
	float3 T = IN.tangent;
	if (!IN.isFrontFace)
	{
		N = -N;
	}
	N = normalize(N);
	N = normalize(N + T * IN.uv.x * 0.8);

	float3 camToP = IN.worldPosition - camera.position.xyz;
	float pixelDepth = dot(camera.forward.xyz, camToP);

#if 1
	N = lerp(N, float3(0.f, 1.f, 0.f), smoothstep(80.f, 100.f, pixelDepth) * 0.4f);
#endif

	surface_info surface;

	surface.albedo = float4(lerp(pow(float3(121, 208, 33) / 255, 2.2), pow(float3(193, 243, 118) / 255, 2.2), IN.uv.y), 1.f);
	surface.N = N;
	surface.roughness = 0.9f;
	surface.roughness = clamp(surface.roughness, 0.01f, 0.99f);
	surface.metallic = 0.f;
	surface.emission = 0.f;

	surface.P = IN.worldPosition;
	surface.V = -normalize(camToP);

	surface.inferRemainingProperties();


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

		float sss = sssTexture.SampleLevel(clampSampler, IN.screenPosition.xy * camera.invScreenDims, 0);
		visibility *= sss;

		[branch]
		if (visibility > 0.f)
		{
			totalLighting.add(calculateDirectLighting(surface, light), visibility);


			// Subsurface scattering.
			{
				// https://www.alanzucconi.com/2017/08/30/fast-subsurface-scattering-1/
				const float distortion = 0.4f;
				float3 sssH = L + N * distortion;
				float sssVdotH = saturate(dot(surface.V, -sssH));

				float sssIntensity = sssVdotH;
				totalLighting.diffuse += sssIntensity.x * lighting.sun.radiance * visibility;
			}
		}
	}




	// Ambient light.
	float2 screenUV = IN.screenPosition.xy * camera.invScreenDims;
	float ao = aoTexture.SampleLevel(clampSampler, screenUV, 0);

	float4 ssr = ssrTexture.SampleLevel(clampSampler, screenUV, 0);


	ambient_factors factors = getAmbientFactors(surface);
	totalLighting.diffuse += diffuseIBL(factors.kd, surface, irradianceTexture, clampSampler) * lighting.globalIlluminationIntensity * ao;
	float3 specular = specularIBL(factors.ks, surface, prefilteredRadianceTexture, brdf, clampSampler);

	specular = lerp(specular, ssr.rgb * surface.F, ssr.a);
	totalLighting.specular += specular * lighting.globalIlluminationIntensity * ao;



	ps_output OUT;
	OUT.hdrColor = totalLighting.evaluate(surface.albedo);
	OUT.worldNormalRoughness = float4(packNormal(surface.N), surface.roughness, 0.f);
	return OUT;

}

