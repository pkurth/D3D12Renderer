
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

[shader("closesthit")]
void radianceClosestHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint3 tri = load3x32BitIndices(meshIndices);

	float2 uvs[] = { meshVertices[tri.x].uv, meshVertices[tri.y].uv, meshVertices[tri.z].uv };
	float3 normals[] = { meshVertices[tri.x].normal, meshVertices[tri.y].normal, meshVertices[tri.z].normal };

	float2 uv = interpolateAttribute(uvs, attribs);
	float3 N = normalize(transformDirectionToWorld(interpolateAttribute(normals, attribs)));

	float3 albedo = (float3)1.f;

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;
	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, constants.frameCount, 16);


#if 0
	float ambientOcclusion = 0.f;

	const uint numAOSamples = constants.numAOSamples;

	for (int i = 0; i < numAOSamples; ++i)
	{
		float3 worldDir = getCosHemisphereSample(randSeed, N);
		ambientOcclusion += traceShadowRay(hitWorldPosition(), worldDir, 1.f, payload.recursion);
	}

	ambientOcclusion /= numAOSamples;

	payload.color = ambientOcclusion.xxx;

#else

	float3 bounceColor = (float3)0.f;

	const uint numSamples = constants.numAOSamples;

	for (int i = 0; i < numSamples; ++i)
	{
		float3 bounceDir = getCosHemisphereSample(randSeed, N);
		float NdotL = saturate(dot(N, bounceDir));
		float sampleProb = NdotL / pi;

		bounceColor += NdotL * traceRadianceRay(hitWorldPosition(), bounceDir, payload.recursion) / sampleProb;
	}

	bounceColor /= numSamples;
	payload.color = (bounceColor * albedo / pi);

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


