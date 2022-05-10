#include "../common/camera.hlsli"
#include "../common/brdf.hlsli"
#include "../common/material.hlsli"
#include "../common/random.hlsli"
#include "../common/light_source.hlsli"
#include "../common/raytracing.hlsli"
#include "../rs/light_probe_rs.hlsli"

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

RWTexture2D<float3> radianceOutput			: register(u0);
RWTexture2D<float4> directionDepthOutput	: register(u1);

ConstantBuffer<light_probe_trace_cb> cb		: register(b0);

SamplerState wrapSampler					: register(s0);


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
	float distance;
};

struct shadow_ray_payload
{
	float visible;
};


static float4 traceRadianceRay(float3 origin, float3 direction)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = 10000.f;

	radiance_ray_payload payload = { float3(0.f, 0.f, 0.f), -1.f };

	TraceRay(rtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,				// Cull mask.
		RADIANCE,			// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		RADIANCE,			// Miss index.
		ray,
		payload);

	return float4(payload.color, payload.distance);
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





// ----------------------------------------
// RAY GENERATION
// ----------------------------------------


//	https://www.lgdv.tf.fau.de/publications/spherical-fibonacci-mapping/
static float3 sphericalFibonacci(float i, float n)
{
	const float PHI = sqrt(5.f) * 0.5f + 0.5f;
#   define madfrac(a, b) ((a)*(b)-floor((a)*(b)))
	float phi = 2.f * M_PI * madfrac(i, PHI - 1);
	float cosTheta = 1.f - (2.f * i + 1.f) * (1.f / n);
	float sinTheta = sqrt(saturate(1.f - cosTheta * cosTheta));

	float sinPhi, cosPhi;
	sincos(phi, sinPhi, cosPhi);

	return float3(
		cosPhi * sinTheta,
		sinPhi * sinTheta,
		cosTheta);

#   undef madfrac
}


[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	uint rayID = launchIndex.x;
	uint probeID = launchIndex.y;

	float3 probeIndex = linearIndexTo3DIndex(probeID, cb.countX, cb.countY);

	float3 origin = probeIndex * cb.cellSize + cb.minCorner;
	float3 direction = sphericalFibonacci(rayID, NUM_RAYS_PER_PROBE);

	float4 radianceAndDistance = traceRadianceRay(origin, direction);
	radianceOutput[launchIndex.xy] = radianceAndDistance.xyz;
	directionDepthOutput[launchIndex.xy] = float4(direction, radianceAndDistance.w);
}


// TODO: Take sun as parameter.


// ----------------------------------------
// RADIANCE
// ----------------------------------------

[shader("closesthit")]
void radianceClosestHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	//uint3 tri = load3x32BitIndices(meshIndices);
	uint3 tri = load3x16BitIndices(meshIndices);

	// Interpolate vertex attributes over triangle.
	float2 uvs[] = { meshVertices[tri.x].uv, meshVertices[tri.y].uv, meshVertices[tri.z].uv };
	float3 normals[] = { meshVertices[tri.x].normal, meshVertices[tri.y].normal, meshVertices[tri.z].normal };

	float2 uv = interpolateAttribute(uvs, attribs);

	surface_info surface;
	surface.N = normalize(transformDirectionToWorld(interpolateAttribute(normals, attribs))); // We ignore normal maps.
	surface.V = -WorldRayDirection();
	surface.P = hitWorldPosition();

	uint mipLevel = 0;
	uint flags = material.getFlags();

	surface.albedo = (((flags & USE_ALBEDO_TEXTURE)
		? albedoTex.SampleLevel(wrapSampler, uv, mipLevel)
		: float4(1.f, 1.f, 1.f, 1.f))
		* unpackColor(material.albedoTint));

	surface.roughness = (flags & USE_ROUGHNESS_TEXTURE)
		? roughTex.SampleLevel(wrapSampler, uv, mipLevel)
		: material.getRoughnessOverride();

	surface.metallic = (flags & USE_METALLIC_TEXTURE)
		? metalTex.SampleLevel(wrapSampler, uv, mipLevel)
		: material.getMetallicOverride();

	surface.emission = material.emission;
	surface.roughness = clamp(surface.roughness, 0.01f, 0.99f);

	surface.inferRemainingProperties();



	float3 L = -normalize(float3(-0.6f, -1.f, -0.3f));
	float sunVisibility = traceShadowRay(surface.P, L, 10000.f);

	light_info light;
	light.initialize(surface, L, float3(1.f, 0.93f, 0.76f) * 50.f);

	light_contribution totalLighting = { float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f) };
	totalLighting.add(calculateDirectLighting(surface, light), sunVisibility);

	payload.color = totalLighting.evaluate(surface.albedo) + surface.emission;
	payload.distance = RayTCurrent();
}

[shader("miss")]
void radianceMiss(inout radiance_ray_payload payload)
{
	payload.color = sky.SampleLevel(wrapSampler, WorldRayDirection(), 0).xyz;
}





// ----------------------------------------
// SHADOW
// ----------------------------------------

[shader("miss")]
void shadowMiss(inout shadow_ray_payload payload)
{
	payload.visible = 1.f;
}


