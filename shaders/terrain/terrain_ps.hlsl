#include "terrain_rs.hlsli"
#include "normal.hlsli"
#include "random.hlsli"
#include "camera.hlsli"
#include "brdf.hlsli"
#include "lighting.hlsli"

Texture2D<float2> normals					: register(t1);
SamplerState clampSampler					: register(s0);
SamplerState wrapSampler					: register(s1);
SamplerComparisonState shadowSampler					: register(s2);

ConstantBuffer<terrain_cb> terrain			: register(b1);

ConstantBuffer<camera_cb> camera			: register(b1, space1);
ConstantBuffer<lighting_cb> lighting		: register(b2, space1);

Texture2D<float4> groundAlbedoTexture		: register(t0, space1);
Texture2D<float3> groundNormalTexture		: register(t1, space1);
Texture2D<float1> groundRoughnessTexture	: register(t2, space1);
Texture2D<float4> rockAlbedoTexture			: register(t3, space1);
Texture2D<float3> rockNormalTexture			: register(t4, space1);
Texture2D<float1> rockRoughnessTexture		: register(t5, space1);


TextureCube<float4> irradianceTexture					: register(t0, space2);
TextureCube<float4> prefilteredRadianceTexture			: register(t1, space2);

Texture2D<float2> brdf									: register(t2, space2);

Texture2D<float> shadowMap								: register(t3, space2);

Texture2D<float> aoTexture								: register(t4, space2);
Texture2D<float> sssTexture								: register(t5, space2);
Texture2D<float4> ssrTexture							: register(t6, space2);


struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3 worldPosition	: POSITION;

	float4 screenPosition	: SV_POSITION;
};

struct ps_output
{
	float4 hdrColor				: SV_Target0;
	float4 worldNormalRoughness	: SV_Target1;
};

struct triplanar_mapping
{
	float2 uvX, uvY, uvZ;
	float3 weights;


	void initialize(float3 position, float3 normal, float3 textureScale, float sharpness)
	{
		uvX = position.zy * textureScale.x;
		uvY = position.xz * textureScale.y;
		uvZ = position.xy * textureScale.z;

		weights = pow(abs(normal), sharpness);
		weights /= dot(weights, 1.f);
	}

	float3 normalmap(float3 normal, float3 tnormalX, float3 tnormalY, float3 tnormalZ)
	{
		// Whiteout blend: https://bgolus.medium.com/normal-mapping-for-a-triplanar-shader-10bf39dca05a

		tnormalX = float3(
			tnormalX.xy + normal.zy,
			abs(tnormalX.z) * normal.x
			);
		tnormalY = float3(
			tnormalY.xy + normal.xz,
			abs(tnormalY.z) * normal.y
			);
		tnormalZ = float3(
			tnormalZ.xy + normal.xy,
			abs(tnormalZ.z) * normal.z
			);

		return normalize(
			tnormalX.zyx * weights.x +
			tnormalY.xzy * weights.y +
			tnormalZ.xyz * weights.z
		);
	}
};

[earlydepthstencil]
[RootSignature(TERRAIN_RS)]
ps_output main(ps_input IN)
{
	float2 n = normals.Sample(clampSampler, IN.uv) * terrain.amplitudeScale;
	float3 N = normalize(float3(n.x, 1.f, n.y));
	
	float groundTexScale = 0.3f;
	float rockTexScale = 0.1f;

	triplanar_mapping tri;
	tri.initialize(IN.worldPosition, N, float3(rockTexScale, groundTexScale, rockTexScale), 15.f);


	float2 groundUV = tri.uvY;

#if 1
	float2 tileUV = IN.uv + 0.1f * float2(fbm(IN.uv * 13.f, 2).x, fbm(IN.uv * 15.f, 3).x);
	float2 tileID = floor(tileUV * 20.f) / 20.f;
	float tileRotation = random(tileID) * M_PI * 2.f;

	float sinRotation, cosRotation;
	sincos(tileRotation, sinRotation, cosRotation);

	groundUV = float2(cosRotation * tri.uvY.x - sinRotation * tri.uvY.y, sinRotation * tri.uvY.x + cosRotation * tri.uvY.y);
#endif
	
	float4 albedo =
		rockAlbedoTexture.Sample(wrapSampler, tri.uvX) * tri.weights.x +
		groundAlbedoTexture.Sample(wrapSampler, groundUV) * tri.weights.y +
		rockAlbedoTexture.Sample(wrapSampler, tri.uvZ) * tri.weights.z;

	float roughness =
		rockRoughnessTexture.Sample(wrapSampler, tri.uvX) * tri.weights.x +
		groundRoughnessTexture.Sample(wrapSampler, groundUV) * tri.weights.y +
		rockRoughnessTexture.Sample(wrapSampler, tri.uvZ) * tri.weights.z;


	float3 tnormalX = sampleNormalMap(rockNormalTexture, wrapSampler, tri.uvX);
	float3 tnormalY = sampleNormalMap(groundNormalTexture, wrapSampler, groundUV);
	float3 tnormalZ = sampleNormalMap(rockNormalTexture, wrapSampler, tri.uvZ);

	N = tri.normalmap(N, tnormalX, tnormalY, tnormalZ);


	surface_info surface;

	surface.albedo = albedo;
	surface.N = N;
	surface.roughness = roughness;
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

		float sss = sssTexture.SampleLevel(clampSampler, IN.screenPosition.xy * camera.invScreenDims, 0);
		visibility *= sss;

		[branch]
		if (visibility > 0.f)
		{
			totalLighting.add(calculateDirectLighting(surface, light), visibility);
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



	//float value = step(0.9f, N.y);
	//col = value.xxx;


	ps_output OUT;
	OUT.hdrColor = totalLighting.evaluate(surface.albedo);
	OUT.worldNormalRoughness = float4(packNormal(N), surface.roughness, 0.f);

	OUT.hdrColor.rgb *= 0.3f;

	return OUT;
}
