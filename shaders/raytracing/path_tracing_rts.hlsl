
#define HLSL 
#define mat4 float4x4
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define uint32 uint

#include "../common/camera.hlsli"
#include "../common/raytracing.hlsli"
#include "../common/brdf.hlsli"
#include "../common/material.hlsli"
#include "../common/random.hlsli"

// Raytracing intrinsics: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-system-values
// Ray flags: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-flags

struct mesh_vertex
{
	float3 position;
	float2 uv;
	float3 normal;
	float3 tangent;
};


// Global.
RaytracingAccelerationStructure rtScene		: register(t0);
TextureCube<float4> sky						: register(t1);

RWTexture2D<float4> output					: register(u0);

ConstantBuffer<camera_cb> camera			: register(b0);
ConstantBuffer<path_tracing_cb> constants	: register(b1);

SamplerState wrapSampler					: register(s0);


// Radiance hit group.
ConstantBuffer<pbr_material_cb> material	: register(b0, space1);
StructuredBuffer<mesh_vertex> meshVertices	: register(t0, space1);
ByteAddressBuffer meshIndices				: register(t1, space1);
Texture2D<float4> albedoTex					: register(t2, space1);
Texture2D<float3> normalTex					: register(t3, space1);
Texture2D<float> roughTex					: register(t4, space1);
Texture2D<float> metalTex					: register(t5, space1);


#define RADIANCE_RAY	0
#define SHADOW_RAY		1

#define NUM_RAY_TYPES	2

struct radiance_ray_payload
{
	float3 color;
	uint recursion;
};

struct shadow_ray_payload
{
	float visible;
};


#define MAX_RECURSION_DEPTH 3 // 0-based.


static float3 traceRadianceRay(float3 origin, float3 direction, uint recursion)
{
	if (recursion >= MAX_RECURSION_DEPTH)
	{
		return float3(0, 0, 0);
	}

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = 10000.f;

	radiance_ray_payload payload = { float3(0.f, 0.f, 0.f), recursion + 1 };

	TraceRay(rtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,				// Cull mask.
		RADIANCE_RAY,		// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		RADIANCE_RAY,		// Miss index.
		ray,
		payload);

	return payload.color;
}

static float traceShadowRay(float3 origin, float3 direction, float distance, uint recursion) // This shader type is also used for ambient occlusion. Just set the distance to something small.
{
	if (recursion >= MAX_RECURSION_DEPTH)
	{
		return 1.f;
	}

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = distance;

	shadow_ray_payload payload = { 0.f };

	TraceRay(rtScene,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, // No need to invoke closest hit shader.
		0xFF,				// Cull mask.
		SHADOW_RAY,			// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		SHADOW_RAY,			// Miss index.
		ray,
		payload);

	return payload.visible;
}





// ----------------------------------------
// RAY GENERATION
// ----------------------------------------

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	float3 origin = camera.position.xyz;

	uint numSamples = constants.numSamplesPerPixel;
	
	float3 color = (float3)0.f;
	for (uint i = 0; i < numSamples; ++i)
	{
		float2 uv = (float2(launchIndex.xy) + halton23(constants.frameCount + i)) / float2(launchDim.xy);
		float3 direction = normalize(restoreWorldDirection(camera.invViewProj, uv, origin));
		color += traceRadianceRay(origin, direction, 0);
	}
	color *= 1.f / (float)numSamples;

	float3 previousColor = output[launchIndex.xy].xyz;
	float previousCount = (float)constants.numAccumulatedFrames;
	float3 newColor = (previousCount * previousColor + color) / (previousCount + 1);

	output[launchIndex.xy] = float4(newColor, 1.f);
}




// ----------------------------------------
// RADIANCE
// ----------------------------------------


static float3 calculateIndirectLighting(inout uint randSeed, float3 position, float3 albedo, float3 F0, float3 N, float3 V, float roughness, float metallic, uint recursion)
{
	// We have to decide whether we sample our diffuse or specular/ggx lobe.
	float probDiffuse = roughness;// probabilityToSampleDiffuse(kD, kS);
	float chooseDiffuse = (nextRand(randSeed) < probDiffuse);

	if (chooseDiffuse)
	{
		// Shoot a randomly selected cosine-sampled diffuse ray.
		float3 L = getCosHemisphereSample(randSeed, N);
		float3 bounceColor = traceRadianceRay(position, L, recursion);

		// Accumulate the color: (NdotL * incomingLight * dif / pi) 
		// Probability of sampling:  (NdotL / pi) * probDiffuse
		return bounceColor * albedo / probDiffuse;
	}
	else
	{
		// Randomly sample the NDF to get a microfacet in our BRDF to reflect off of
		float3 H = importanceSampleGGX(randSeed, N, roughness);

		// Compute the outgoing direction based on this (perfectly reflective) microfacet
		float3 L = reflect(-V, H);

		// Compute our color by tracing a ray in this direction
		float3 bounceColor = traceRadianceRay(position, L, recursion);

		float NdotV = saturate(dot(N, V));
		float NdotL = saturate(dot(N, L));
		float NdotH = saturate(dot(N, H));
		float LdotH = saturate(dot(L, H));

		float NDF = distributionGGX(NdotH, roughness);
		float  G = geometrySmith(NdotL, NdotV, roughness);
		float3 F = fresnelSchlick(LdotH, F0);

		float3 numerator = NDF * G * F;
		float denominator = 4.f * NdotV * NdotL;
		float3 specular = numerator / max(denominator, 0.001f);

		// What's the probability of sampling vector H?
		float  ggxProb = max(NDF * NdotH / (4.f * LdotH), 0.01f);

		// Accumulate the color:  ggx-BRDF * incomingLight * NdotL / probability-of-sampling
		//    -> Should really simplify the math above.
		return NdotL * bounceColor * specular / (ggxProb * (1.f - probDiffuse));
	}
}


[shader("closesthit")]
void radianceClosestHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint3 tri = load3x32BitIndices(meshIndices);

	float2 uvs[] = { meshVertices[tri.x].uv, meshVertices[tri.y].uv, meshVertices[tri.z].uv };
	float3 normals[] = { meshVertices[tri.x].normal, meshVertices[tri.y].normal, meshVertices[tri.z].normal };

	float2 uv = interpolateAttribute(uvs, attribs);
	float3 N = normalize(transformDirectionToWorld(interpolateAttribute(normals, attribs)));

#if 1
	float3 albedo = (float3)1.f;
	float roughness = 0.5f;
	float metallic = 0.f;
#else
	uint mipLevel = 0;
	uint flags = material.flags;

	float4 albedo = ((flags & USE_ALBEDO_TEXTURE)
		? albedoTex.SampleLevel(wrapSampler, uv, mipLevel)
		: float4(1.f, 1.f, 1.f, 1.f))
		* material.albedoTint;

	// We ignore normal maps for now.

	float roughness = (flags & USE_ROUGHNESS_TEXTURE)
		? roughTex.SampleLevel(wrapSampler, uv, mipLevel)
		: getRoughnessOverride(material);
	roughness = clamp(roughness, 0.01f, 0.99f);

	float metallic = (flags & USE_METALLIC_TEXTURE)
		? metalTex.SampleLevel(wrapSampler, uv, mipLevel)
		: getMetallicOverride(material);
#endif


	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;
	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, constants.frameCount, 16);


#if 0
	float3 worldDir = getCosHemisphereSample(randSeed, N);
	payload.color = traceShadowRay(hitWorldPosition(), worldDir, 1.f, payload.recursion).xxx;

#else
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	payload.color = calculateIndirectLighting(randSeed, hitWorldPosition(), albedo, F0, N, -WorldRayDirection(), roughness, metallic, payload.recursion);

#endif
}

[shader("miss")]
void radianceMiss(inout radiance_ray_payload payload)
{
	payload.color = sky.SampleLevel(wrapSampler, WorldRayDirection(), 0).xyz;
}

[shader("anyhit")]
void radianceAnyHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	AcceptHitAndEndSearch();
}




// ----------------------------------------
// SHADOW
// ----------------------------------------

[shader("closesthit")]
void shadowClosestHit(inout shadow_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	// This shader will never be called.
}

[shader("miss")]
void shadowMiss(inout shadow_ray_payload payload)
{
	payload.visible = 1.f;
}

[shader("anyhit")]
void shadowAnyHit(inout shadow_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	AcceptHitAndEndSearch();
}


