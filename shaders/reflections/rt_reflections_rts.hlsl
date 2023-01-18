#include "../common/camera.hlsli"
#include "../common/raytracing.hlsli"
#include "../rs/rt_reflections_rs.hlsli"
#include "../rs/ssr_rs.hlsli"
#include "../common/light_source.hlsli"
#include "../common/lighting.hlsli"
#include "../common/brdf.hlsli"
#include "../common/normal.hlsli"
#include "../common/material.hlsli"
#include "../common/procedural_sky.hlsli"

// Raytracing intrinsics: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-system-values
// Ray flags: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-flags

struct mesh_vertex
{
	float2 uv;
	float3 normal;
	float3 tangent;
};


// Global.
RaytracingAccelerationStructure rtScene		: register(t0);
TextureCube<float4> sky						: register(t1);

Texture2D<float3> probeIrradiance			: register(t2);
Texture2D<float2> probeDepth				: register(t3);

Texture2D<float> depthBuffer                : register(t4);
Texture2D<float3> worldNormalsRoughness		: register(t5);
Texture2D<float2> noise						: register(t6);
Texture2D<float2> motion					: register(t7);

RWTexture2D<float4> output					: register(u0);

ConstantBuffer<rt_reflections_cb> cb		: register(b0);
ConstantBuffer<camera_cb> camera			: register(b1);
ConstantBuffer<lighting_cb> lighting		: register(b2);
SamplerState linearSampler					: register(s0);
SamplerState pointSampler					: register(s1);


// Radiance hit group.
ConstantBuffer<pbr_material_cb> material	: register(b0, space1);
StructuredBuffer<mesh_vertex> meshVertices	: register(t0, space1);
ByteAddressBuffer meshIndices				: register(t1, space1);
Texture2D<float4> albedoTex					: register(t2, space1);
Texture2D<float3> normalTex					: register(t3, space1);
Texture2D<float> roughTex					: register(t4, space1);
Texture2D<float> metalTex					: register(t5, space1);


struct radiance_ray_payload
{
	float3 color;
};

struct shadow_ray_payload
{
	float visible;
};

static float3 traceRadianceRay(float3 origin, float3 direction, uint recursion)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = 10000.f;

	radiance_ray_payload payload = { float3(0.f, 0.f, 0.f) };

	TraceRay(rtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,				// Cull mask.
		RADIANCE,			// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		RADIANCE,			// Miss index.
		ray,
		payload);

	return payload.color;
}

static float traceShadowRay(float3 origin, float3 direction, float distance)
{
#ifdef SHADOW
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = distance;

	shadow_ray_payload payload = { 0.f };

	TraceRay(rtScene,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, // No need to invoke closest hit shader.
		0xFF,				// Cull mask.
		SHADOW,				// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		SHADOW,				// Miss index.
		ray,
		payload);

	return payload.visible;
#else
	return 1.f;
#endif
}

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	float2 uv = float2(launchIndex.xy) / float2(launchDim.xy);
    const float depth = depthBuffer.SampleLevel(pointSampler, uv, 0);

	if (depth == 1.f)
	{
		output[launchIndex.xy] = float4(0.f, 0.f, -1.f, 0.f);
		return;
	}

	const float2 uvM = motion.SampleLevel(linearSampler, uv, 0);
	const float3 normalAndRoughness = worldNormalsRoughness.SampleLevel(linearSampler, uv + uvM, 0);

	if (all(normalAndRoughness.xy == float2(0.f, 0.f)))
	{
		// If normal is zero length, just return no hit.
		// This happens if the window is resized, because we are sampling the last frame's normals.
		output[launchIndex.xy] = float4(0.f, 0.f, -1.f, 0.f);
		return;
	}

	const float3 normal = unpackNormal(normalAndRoughness.xy);
	const float roughness = clamp(normalAndRoughness.z * normalAndRoughness.z * normalAndRoughness.z, 0.03f, 0.97f); // Raising the roughness to a power gets rid of some jittering.

	float3 origin = restoreWorldSpacePosition(camera.invViewProj, uv, depth);
	float3 viewDir = normalize(origin - camera.position.xyz);

	float2 h = halton23(cb.frameIndex & 31);
	uint3 noiseDims;
	noise.GetDimensions(0, noiseDims.x, noiseDims.y, noiseDims.z);
	float2 Xi = noise.SampleLevel(linearSampler, (uv + h) * float2(launchDim.xy) / float2(noiseDims.xy), 0);
	Xi.y = lerp(Xi.y, 0.f, SSR_GGX_IMPORTANCE_SAMPLE_BIAS);

	float4 H = importanceSampleGGX(Xi, normal, roughness);
	float3 direction = reflect(viewDir, H.xyz);

	float3 color = traceRadianceRay(origin, direction, 0);

	output[launchIndex.xy] = float4(color, 1);
}

// ----------------------------------------
// RADIANCE
// ----------------------------------------

[shader("closesthit")]
void radianceClosestHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint flags = material.getFlags();

	uint3 tri = (flags & MATERIAL_USE_32_BIT_INDICES) ? load3x32BitIndices(meshIndices) : load3x16BitIndices(meshIndices);

	// Interpolate vertex attributes over triangle.
	float2 uvs[] = { meshVertices[tri.x].uv, meshVertices[tri.y].uv, meshVertices[tri.z].uv };
	float3 normals[] = { meshVertices[tri.x].normal, meshVertices[tri.y].normal, meshVertices[tri.z].normal };

	float2 uv = interpolateAttribute(uvs, attribs);

	surface_info surface;
	surface.N = normalize(transformDirectionToWorld(interpolateAttribute(normals, attribs))); // We ignore normal maps.
	surface.V = -WorldRayDirection();
	surface.P = hitWorldPosition();

	uint mipLevel = 3;

	surface.albedo = (((flags & MATERIAL_USE_ALBEDO_TEXTURE)
		? albedoTex.SampleLevel(linearSampler, uv, mipLevel)
		: float4(1.f, 1.f, 1.f, 1.f))
		* unpackColor(material.albedoTint));

	surface.roughness = (flags & MATERIAL_USE_ROUGHNESS_TEXTURE)
		? roughTex.SampleLevel(linearSampler, uv, mipLevel)
		: material.getRoughnessOverride();

	surface.metallic = (flags & MATERIAL_USE_METALLIC_TEXTURE)
		? metalTex.SampleLevel(linearSampler, uv, mipLevel)
		: material.getMetallicOverride();

	surface.emission = material.emission;
	surface.roughness = clamp(surface.roughness, 0.01f, 0.99f);

	surface.inferRemainingProperties();



	float3 L = -lighting.sun.direction;
	float sunVisibility = traceShadowRay(surface.P, L, 10000.f);

	light_info light;
	light.initialize(surface, L, lighting.sun.radiance);

	light_contribution totalLighting = { float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f) };
	totalLighting.add(calculateDirectLighting(surface, light), sunVisibility);

	totalLighting.diffuse += lighting.lightProbeGrid.sampleIrradianceAtPosition(surface.P, surface.N, probeIrradiance, probeDepth, linearSampler);

	payload.color = totalLighting.evaluate(surface.albedo).rgb + surface.emission;
}

[shader("miss")]
void radianceMiss(inout radiance_ray_payload payload)
{
	if(cb.sampleSkyFromTexture == 1)
	{
		payload.color = sky.SampleLevel(linearSampler, WorldRayDirection(), 0).xyz;
	}
	else
	{
		payload.color = proceduralSkySimple(normalize(WorldRayDirection()), normalize(-lighting.sun.direction));
	}
}



// ----------------------------------------
// SHADOW
// ----------------------------------------

[shader("miss")]
void shadowMiss(inout shadow_ray_payload payload)
{
	payload.visible = 1.f;
}
