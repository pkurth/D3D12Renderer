
#define HLSL 
#define mat4 float4x4
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define uint32 uint

#include "common/camera.hlsl"
#include "common/raytracing.hlsl"

// Raytracing intrinsics: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-system-values
// Ray flags: https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#ray-flags

struct mesh_vertex
{
	float3 position;
	float2 uv;
	float3 normal;
};


RaytracingAccelerationStructure rtScene		: register(t0); // Global.
ConstantBuffer<camera_cb> camera			: register(b0);	// Global.
ConstantBuffer<raytracing_cb> raytracing	: register(b1);	// Global.

RWTexture2D<float4> output					: register(u0);	// Raygen.


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

static float3 traceRadianceRay(float3 origin, float3 direction, uint recursion)
{
	/*if (recursion >= raytracing.maxRecursionDepth)
	{
		return float3(0, 0, 0);
	}*/

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = 100000.f;

	radiance_ray_payload payload = { float3(0.f, 0.f, 0.f), recursion + 1 };

	TraceRay(rtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,				// Cull mask.
		RADIANCE_RAY,		// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		0,
		ray,
		payload);

	return payload.color;
}

static bool traceShadowRay(float3 origin, float3 direction, uint recursion)
{
	/*if (recursion >= raytracing.maxRecursionDepth)
	{
		return false;
	}*/

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
		1,
		ray,
		payload);

	return payload.hitGeometry;
}

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	float3 origin = float3(0.f, 0.f, 0.f);

	//float2 uv = float2(launchIndex.xy) / float2(launchDim.xy);
	//float3 direction = normalize(restoreWorldSpaceDirection(camera.invViewProj, uv, camera.position, camera.nearPlane));
	float3 direction = float3(0.f, 0.f, -1.f);
	float3 color = traceRadianceRay(origin, direction, 0);
	
	output[launchIndex.xy] = float4(color, 1);
}

[shader("miss")]
void radianceMiss(inout radiance_ray_payload payload)
{
	payload.color = float3(0.f, 0.f, 0.f);
}

[shader("closesthit")]
void radianceClosestHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 1.f, 1.f);
}

[shader("anyhit")]
void radianceAnyHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	//IgnoreHit(); // Test.
	AcceptHitAndEndSearch();
}

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
