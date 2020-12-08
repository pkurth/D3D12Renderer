
#define HLSL 
#define mat4 float4x4
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define uint32 uint

#include "../common/camera.hlsl"
#include "../common/raytracing.hlsl"
#include "../common/light_source.hlsl"
#include "../common/brdf.hlsl"
#include "../common/normal.hlsl"
#include "../common/material.hlsl"

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
ConstantBuffer<camera_cb> camera			: register(b0);
ConstantBuffer<directional_light_cb> sun	: register(b1);
ConstantBuffer<raytracing_cb> raytracing	: register(b2);
RWTexture2D<float4> output					: register(u0);
SamplerState wrapSampler					: register(s0);
SamplerState clampSampler					: register(s1);

// Gbuffer (global).
Texture2D<float> depthBuffer                : register(t0, space1);
Texture2D<float2> worldNormals				: register(t1, space1);

// Environment (global).
TextureCube<float4> irradianceTexture		: register(t2, space1);
TextureCube<float4> environmentTexture		: register(t3, space1);
TextureCube<float4> sky						: register(t4, space1);
Texture2D<float4> brdf						: register(t5, space1);

// Radiance hit group.
ConstantBuffer<pbr_material_cb> material	: register(b0, space2);
StructuredBuffer<mesh_vertex> meshVertices	: register(t0, space2);
ByteAddressBuffer meshIndices				: register(t1, space2);
Texture2D<float4> albedoTex					: register(t2, space2);
Texture2D<float3> normalTex					: register(t3, space2);
Texture2D<float> roughTex					: register(t4, space2);
Texture2D<float> metalTex					: register(t5, space2);


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
	bool hitGeometry;
};

static float3 sampleEnvironment(float3 direction)
{
	return sky.SampleLevel(wrapSampler, direction, 0).xyz * raytracing.skyIntensity;
}

static float3 traceRadianceRay(float3 origin, float3 direction, uint recursion)
{
	if (recursion >= raytracing.maxRecursionDepth)
	{
		return float3(0, 0, 0);
	}

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = raytracing.maxRayDistance;

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

static bool traceShadowRay(float3 origin, float3 direction, uint recursion)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = 100000.f;

	shadow_ray_payload payload = { true };

	TraceRay(rtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_FORCE_OPAQUE,
		0xFF,				// Cull mask.
		SHADOW_RAY,			// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		SHADOW_RAY,			// Miss index.
		ray,
		payload);

	return payload.hitGeometry;
}

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	float2 uv = float2(launchIndex.xy) / float2(launchDim.xy);
	float4 depth4 = depthBuffer.Gather(clampSampler, uv);
	uint minComponent =
		(depth4.x < depth4.y) ?
		(depth4.x < depth4.z) ?
		(depth4.x < depth4.w) ? 0 : 3 :
		(depth4.z < depth4.w) ? 2 : 3 :
		(depth4.y < depth4.z) ?
		(depth4.y < depth4.w) ? 1 : 3 :
		(depth4.z < depth4.w) ? 2 : 3;

	float depth = depth4[minComponent];

	float3 color = (float3)0.f;
	if (depth < 1.f)
	{
#if 1
		float3 origin = restoreWorldSpacePosition(camera.invViewProj, uv, depth);
		float3 direction = normalize(origin - camera.position);

		float2 normal2 = float2(
			worldNormals.GatherRed(clampSampler, uv)[minComponent],
			worldNormals.GatherGreen(clampSampler, uv)[minComponent]
		);
		float3 normal = unpackNormal(normal2);
		direction = reflect(direction, normal);
#else
		float3 origin = camera.position.xyz;
		float3 direction = normalize(restoreWorldDirection(camera.invViewProj, uv, origin));
#endif

		color = traceRadianceRay(origin, direction, 0);
	}
	output[launchIndex.xy] = float4(color, 1);
}

// ----------------------------------------
// RADIANCE
// ----------------------------------------

[shader("miss")]
void radianceMiss(inout radiance_ray_payload payload)
{
	payload.color = sampleEnvironment(WorldRayDirection());
}

[shader("closesthit")]
void radianceClosestHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint3 tri = load3x16BitIndices(meshIndices);

	float2 uvs[] = { meshVertices[tri.x].uv, meshVertices[tri.y].uv, meshVertices[tri.z].uv };
	float3 normals[] = { meshVertices[tri.x].normal, meshVertices[tri.y].normal, meshVertices[tri.z].normal };

	float2 uv = interpolateAttribute(uvs, attribs);
	float3 N = transformDirectionToWorld(interpolateAttribute(normals, attribs));

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

	float ao = 1.f;// (flags & USE_AO_TEXTURE) ? RMAO.z : 1.f;



	float3 hitPosition = hitWorldPosition();
	float3 L = -sun.direction;
	float visibility = 1.f - (float)traceShadowRay(hitPosition, L, payload.recursion);
	
	float3 radiance = sun.radiance * visibility; // No attenuation for sun.
	float3 V = -WorldRayDirection();
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	
	payload.color = calculateDirectLighting(albedo, radiance, N, L, V, F0, roughness, metallic);
	payload.color += calculateAmbientLighting(albedo, irradianceTexture, environmentTexture, brdf, clampSampler, N, V, F0, roughness, metallic, ao) * raytracing.environmentIntensity;

	float3 reflectionDirection = normalize(reflect(WorldRayDirection(), N));
	float3 bounceRadiance = traceRadianceRay(hitPosition, reflectionDirection, payload.recursion);
	payload.color += calculateDirectLighting(albedo, bounceRadiance, N, reflectionDirection, V, F0, roughness, metallic);

	float t = RayTCurrent();
	if (t > raytracing.fadeoutDistance)
	{
		float3 env = sampleEnvironment(WorldRayDirection());
		payload.color = lerp(payload.color, env, (t - raytracing.fadeoutDistance) / (raytracing.maxRayDistance - raytracing.fadeoutDistance));
	}
}

[shader("anyhit")]
void radianceAnyHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	AcceptHitAndEndSearch();
}



// ----------------------------------------
// SHADOW
// ----------------------------------------

[shader("miss")]
void shadowMiss(inout shadow_ray_payload payload)
{
	payload.hitGeometry = false;
}

[shader("closesthit")]
void shadowClosestHit(inout shadow_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.hitGeometry = true;
}

[shader("anyhit")]
void shadowAnyHit(inout shadow_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	AcceptHitAndEndSearch();
}
