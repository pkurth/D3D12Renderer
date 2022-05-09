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

RWTexture2D<float4> output					: register(u0);

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
};

struct shadow_ray_payload
{
	float visible;
};


static float3 traceRadianceRay(float3 origin, float3 direction)
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





// ----------------------------------------
// RAY GENERATION
// ----------------------------------------

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	float3 color = float3(1.f, 0.f, 1.f);

	if (launchIndex.x % LIGHT_PROBE_TOTAL_RESOLUTION == 0
		|| launchIndex.x % LIGHT_PROBE_TOTAL_RESOLUTION == 7
		|| launchIndex.y % LIGHT_PROBE_TOTAL_RESOLUTION == 0
		|| launchIndex.y % LIGHT_PROBE_TOTAL_RESOLUTION == 7)
	{
		// Border pixel.
	}
	else
	{
		uint2 probeIndex2 = launchIndex.xy / LIGHT_PROBE_TOTAL_RESOLUTION;
		uint3 probeIndex = uint3(probeIndex2.x % cb.countX, probeIndex2.x / cb.countX, probeIndex2.y);
		
		uint2 pixelIndex = launchIndex.xy % LIGHT_PROBE_TOTAL_RESOLUTION; // [1, 6]
		pixelIndex -= 1; // Subtract the border -> [0, 5]

		float2 uv = ((float2)pixelIndex + 0.5f) / LIGHT_PROBE_RESOLUTION;
		float2 oct = uv * 2.f - 1.f;

		float3 direction = decodeOctahedral(oct);
		float3 origin = probeIndex * cb.cellSize + cb.minCorner;

		//color = probeIndex.z / 10.f;
		color = traceRadianceRay(origin, direction);
	}

	output[launchIndex.xy] = float4(color, 1.f);
}




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
}

[shader("miss")]
void radianceMiss(inout radiance_ray_payload payload)
{
	payload.color = float3(0.f, 0.f, 0.f);
}





// ----------------------------------------
// SHADOW
// ----------------------------------------

[shader("miss")]
void shadowMiss(inout shadow_ray_payload payload)
{
	payload.visible = 1.f;
}


