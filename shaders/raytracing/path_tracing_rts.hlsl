
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
#define NUM_RAY_TYPES	1

struct radiance_ray_payload
{
	float3 color;
	uint recursion;
};

static float3 sampleEnvironment(float3 direction)
{
	return sky.SampleLevel(wrapSampler, direction, 0).xyz;
}

static float3 traceRadianceRay(float3 origin, float3 direction, uint recursion)
{
	if (recursion >= 3)
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

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	float3 origin = camera.position.xyz;

	static const uint numSamples = 1;

	float3 color = (float3)0.f;
	for (uint i = 0; i < numSamples; ++i)
	{
		float2 uv = (float2(launchIndex.xy) + halton23(i)) / float2(launchDim.xy);
		float3 direction = normalize(restoreWorldDirection(camera.invViewProj, uv, origin));
		color += traceRadianceRay(origin, direction, 0);
	}
	color *= 1.f / (float)numSamples;

	output[launchIndex.xy] = float4(color, 1.f);
}

[shader("closesthit")]
void radianceClosestHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint3 tri = load3x32BitIndices(meshIndices);

	float2 uvs[] = { meshVertices[tri.x].uv, meshVertices[tri.y].uv, meshVertices[tri.z].uv };
	float3 normals[] = { meshVertices[tri.x].normal, meshVertices[tri.y].normal, meshVertices[tri.z].normal };

	float2 uv = interpolateAttribute(uvs, attribs);
	float3 N = normalize(transformDirectionToWorld(interpolateAttribute(normals, attribs)));

	uint mipLevel = 0;
	uint flags = material.flags;

	float4 albedo = ((flags & USE_ALBEDO_TEXTURE)
		? albedoTex.SampleLevel(wrapSampler, uv, mipLevel)
		: float4(1.f, 1.f, 1.f, 1.f))
		* material.albedoTint;

	float roughness = (flags & USE_ROUGHNESS_TEXTURE)
		? roughTex.SampleLevel(wrapSampler, uv, mipLevel)
		: getRoughnessOverride(material);
	roughness = clamp(roughness, 0.01f, 0.99f);


	static const uint numSamples = 8;

	float3 newOrigin = hitWorldPosition();
	float3 seed = float3(DispatchRaysIndex()) + float3(69823.241278f, 613.12371825f, 123.6819243f);

	float3 color = (float3)0.f;
	for (uint i = 0; i < numSamples; ++i)
	{
		float3 S;
		float l = 0.f;
		do
		{
			S = float3(random(seed.x), random(seed.y), random(seed.z)) * 2.f - (float3)1.f;
			l = length(S);
			seed = S;
		} while (l >= 1.f);

		float3 newDirection = normalize(N + S / l);

		color += traceRadianceRay(newOrigin, newDirection, payload.recursion);
	}

	color /= numSamples;

	payload.color = color * albedo.xyz;
}

[shader("miss")]
void radianceMiss(inout radiance_ray_payload payload)
{
	payload.color = sampleEnvironment(WorldRayDirection());
}

[shader("anyhit")]
void radianceAnyHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	AcceptHitAndEndSearch();
}

