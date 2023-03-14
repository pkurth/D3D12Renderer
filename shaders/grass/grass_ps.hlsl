#include "grass_rs.hlsli"
#include "camera.hlsli"
#include "normal.hlsli"
#include "lighting.hlsli"
#include "depth_only_rs.hlsli"

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


#ifdef NO_DEPTH_PREPASS
ConstantBuffer<depth_only_object_id_cb> id		: register(b3);
#endif


struct ps_input
{
	float2 uv				: TEXCOORDS;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPosition	: POSITION;

#ifdef NO_DEPTH_PREPASS
	float3 ndc				: NDC;
	float3 prevFrameNDC		: PREV_FRAME_NDC;
#endif

	float4 screenPosition	: SV_POSITION;
	bool isFrontFace		: SV_IsFrontFace;
};


struct ps_output
{
	float4 hdrColor				: SV_Target0;
	float4 worldNormalRoughness	: SV_Target1;

#ifdef NO_DEPTH_PREPASS
	float2 screenVelocity		: SV_Target2;
	uint objectID				: SV_Target3;
#endif
};

[earlydepthstencil]
[RootSignature(GRASS_RS)]
ps_output main(ps_input IN)
{
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


	float2 screenUV = IN.screenPosition.xy * camera.invScreenDims;

	light_contribution totalLighting = { float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f) };

	totalLighting.addSunLight(surface, lighting, screenUV, pixelDepth,
		shadowMap, shadowSampler, lighting.shadowMapTexelSize, sssTexture, clampSampler, 0.5f);

	totalLighting.addImageBasedAmbientLighting(surface, irradianceTexture, prefilteredRadianceTexture, brdf, ssrTexture, aoTexture,
		clampSampler, screenUV, lighting.globalIlluminationIntensity);





	surface.roughness = 1.f;

	ps_output OUT;
	OUT.hdrColor = totalLighting.evaluate(surface.albedo);
	OUT.worldNormalRoughness = float4(packNormal(surface.N), surface.roughness, 0.f);


#ifdef NO_DEPTH_PREPASS
	OUT.screenVelocity = screenSpaceVelocity(IN.ndc, IN.prevFrameNDC, camera.jitter, camera.prevFrameJitter);
	OUT.objectID = id.id;
#endif

	return OUT;

}

