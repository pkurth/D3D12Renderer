#include "../common/camera.hlsli"
#include "../common/raytracing.hlsli"
#include "../common/brdf.hlsli"
#include "../common/material.hlsli"
#include "../common/random.hlsli"
#include "../common/light_source.hlsli"

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


struct radiance_ray_payload
{
	float3 color;
	uint recursion;
	uint randSeed;
};

struct shadow_ray_payload
{
	float visible;
};


#define NUM_LIGHTS 3

static const point_light_cb pointLights[NUM_LIGHTS] =
{
	// Position, radius, radiance. The last value (-1) is only useful for rasterization, where this is the index into a list of shadow maps (-1 means no shadows).
	{
		float3(0.f, 3.f, 0.f),
		15.f,
		float3(0.8f, 0.2f, 0.1f) * 50.f,
		-1
	},
	{
		float3(-5.f, 8.f, 0.f),
		15.f,
		float3(0.2f, 0.8f, 0.3f) * 50.f,
		-1
	},
	{
		float3(5.f, 8.f, 0.f),
		15.f,
		float3(0.2f, 0.3f, 0.8f) * 50.f,
		-1
	},
};



static float3 traceRadianceRay(float3 origin, float3 direction, uint randSeed, uint recursion)
{
	// This is replaced by the russian roulette below.
	/*if (recursion >= constants.maxRecursionDepth)
	{
		return float3(0, 0, 0);
	}*/

	// My attempt at writing a russian roulette termination, which guarantees that the recursion depth does not exceed the maximum.
	// Lower numbers make rays terminate earlier, which improves performance, but hurts the convergence speed.
	// I think normally you wouldn't want the termination probability to go to 1, but DirectX will remove the device, if you exceed
	// the recursion limit.
	float russianRouletteFactor = 1.f;
	if (recursion >= constants.startRussianRouletteAfter)
	{
		uint rouletteSteps = constants.maxRecursionDepth - constants.startRussianRouletteAfter + 1;
		uint stepsRemaining = recursion - constants.startRussianRouletteAfter + 1;
		float stopProbability = min(1.f, (float)stepsRemaining / (float)rouletteSteps);

		if (nextRand(randSeed) <= stopProbability)
		{
			return (float3)0.f;
		}

		russianRouletteFactor = 1.f / (1.f - stopProbability);
	}

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.01f;
	ray.TMax = 10000.f;

	radiance_ray_payload payload = { float3(0.f, 0.f, 0.f), recursion + 1, randSeed };

	TraceRay(rtScene,
		RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,				// Cull mask.
		RADIANCE,			// Addend on the hit index.
		NUM_RAY_TYPES,		// Multiplier on the geometry index within a BLAS.
		RADIANCE,			// Miss index.
		ray,
		payload);

	return payload.color * russianRouletteFactor;
}

static float traceShadowRay(float3 origin, float3 direction, float distance, uint recursion) // This shader type is also used for ambient occlusion. Just set the distance to something small.
{
	if (recursion >= constants.maxRecursionDepth)
	{
		return 1.f;
	}

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

	uint randSeed = initRand(launchIndex.x + launchIndex.y * launchDim.x, constants.frameCount, 16);


	// Jitter for anti-aliasing.
	float2 pixelOffset = float2(nextRand(randSeed), nextRand(randSeed));
	float2 uv = (float2(launchIndex.xy) + pixelOffset) / float2(launchDim.xy);

	float3 origin = camera.position.xyz;
	float3 direction = restoreWorldDirection(camera.invViewProj, uv, origin);


	if (constants.useThinLensCamera)
	{
		direction /= camera.projectionParams.x;

		float3 focalPoint = origin + constants.focalLength * direction;

		float2 rnd = float2(2.f * pi * nextRand(randSeed), constants.lensRadius * nextRand(randSeed));
		float2 originOffset = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

		origin += camera.right.xyz * originOffset.x + camera.up.xyz * originOffset.y;
		direction = focalPoint - origin;
	}

	direction = normalize(direction);


	// Trace ray.
	float3 color = traceRadianceRay(origin, direction, randSeed, 0);


	// Blend result color with previous frames.
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
	float probDiffuse = roughness; // I don't think this is correct.
	float chooseDiffuse = (nextRand(randSeed) < probDiffuse);

	if (chooseDiffuse)
	{
		// Shoot a randomly selected cosine-sampled diffuse ray.
		float3 L = getCosHemisphereSample(randSeed, N);
		float3 bounceColor = traceRadianceRay(position, L, randSeed, recursion);

		// Accumulate the color: (NdotL * incomingLight * dif / pi) 
		// Probability of sampling:  (NdotL / pi) * probDiffuse
		return bounceColor * albedo / probDiffuse;
	}
	else
	{
		// Randomly sample the NDF to get a microfacet in our BRDF to reflect off of.
		float3 H = importanceSampleGGX(randSeed, N, roughness);

		// Compute the outgoing direction based on this (perfectly reflective) microfacet.
		float3 L = reflect(-V, H);

		// Compute our color by tracing a ray in this direction.
		float3 bounceColor = traceRadianceRay(position, L, randSeed, recursion);

		float NdotV = saturate(dot(N, V));
		float NdotL = saturate(dot(N, L));
		float NdotH = saturate(dot(N, H));
		float LdotH = saturate(dot(L, H));

		float NDF = distributionGGX(NdotH, roughness);
		float G = geometrySmith(NdotL, NdotV, roughness);
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
	uint3 tri = load3x32BitIndices(meshIndices); // Use load3x16BitIndices, if you have 16-bit indices.

	// Interpolate vertex attributes over triangle.
	float2 uvs[] = { meshVertices[tri.x].uv, meshVertices[tri.y].uv, meshVertices[tri.z].uv };
	float3 normals[] = { meshVertices[tri.x].normal, meshVertices[tri.y].normal, meshVertices[tri.z].normal };

	float2 uv = interpolateAttribute(uvs, attribs);
	float3 N = normalize(transformDirectionToWorld(interpolateAttribute(normals, attribs)));

	float3 emission = (float3)0.f;
	float3 albedo = (float3)1.f;
	float roughness = 1.f;
	float metallic = 0.f;

	if (constants.useRealMaterials)
	{
		uint mipLevel = 0;
		uint flags = material.flags;

		albedo = (((flags & USE_ALBEDO_TEXTURE)
			? albedoTex.SampleLevel(wrapSampler, uv, mipLevel)
			: float4(1.f, 1.f, 1.f, 1.f))
			* unpackColor(material.albedoTint)).xyz;

		// We ignore normal maps for now.

		roughness = (flags & USE_ROUGHNESS_TEXTURE)
			? roughTex.SampleLevel(wrapSampler, uv, mipLevel)
			: getRoughnessOverride(material);

		metallic = (flags & USE_METALLIC_TEXTURE)
			? metalTex.SampleLevel(wrapSampler, uv, mipLevel)
			: getMetallicOverride(material);

		emission = material.emission;
	}

	payload.color = emission;

	roughness = clamp(roughness, 0.01f, 0.99f);

	float3 hitPoint = hitWorldPosition();

	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
	payload.color += calculateIndirectLighting(payload.randSeed, hitPoint, albedo, F0, N, -WorldRayDirection(), roughness, metallic, payload.recursion);


	if (constants.enableDirectLighting)
	{
		// Sun light.
		float3 sunDirection = normalize(float3(-0.6f, -1.f, -0.3f));
		float3 sunRadiance = float3(1.f, 0.93f, 0.76f) * constants.lightIntensityScale * 2.f;

		payload.color +=
			calculateDirectLighting(albedo, sunRadiance, N, -sunDirection, -WorldRayDirection(), F0, roughness, metallic)
			* traceShadowRay(hitPoint, -sunDirection, 10000.f, payload.recursion);



		// Random point light.
		uint lightIndex = min(uint(NUM_LIGHTS * nextRand(payload.randSeed)), NUM_LIGHTS - 1);

		float pointLightRadius = constants.pointLightRadius;
		float3 randomPointOnLight = pointLights[lightIndex].position + getRandomPointOnSphere(payload.randSeed, pointLightRadius);

		float3 pointLightL = randomPointOnLight - hitPoint;
		float distance = length(pointLightL);
		pointLightL /= distance;

		float pointLightVisibility = traceShadowRay(hitPoint, pointLightL, distance, payload.recursion);
		float pointLightSolidAngle = solidAngleOfSphere(pointLightRadius, distance) * 0.5f; // Divide by 2, since we are only interested in hemisphere.
		float3 pointLightColor =
			calculateDirectLighting(albedo, pointLights[lightIndex].radiance * LIGHT_RADIANCE_SCALE * constants.lightIntensityScale, N, pointLightL, -WorldRayDirection(), F0, roughness, metallic)
			* getAttenuation(distance, pointLights[lightIndex].radius)
			* NUM_LIGHTS // Correct for probability of choosing this particular light.
			* pointLightVisibility
			* pointLightSolidAngle; // Correct for probability of "randomly" hitting this light. I *think* this is correct. See https://github.com/NVIDIA/Q2RTX/blob/master/src/refresh/vkpt/shader/light_lists.h#L295

#if 0
		if (pointLightVisibility > 0.f)
		{
			float lightPickPDF = 1.f / NUM_LIGHTS;
			float lightHitPDF = 1.f; // If I understand correctly, this is the probability of hitting the light. However, since we explicitly sample this light, its probability is 1, isn't it?
			float lightPDFWeight = lightHitPDF * lightPickPDF;
			float brdfPDFWeight = ;
			float misWeight = multipleImportanceSampleWeighting(brdfPDFWeight, lightPDFWeight);
			pointLightColor *= misWeight;
		}
#endif

		payload.color += pointLightColor;
	}
}

[shader("miss")]
void radianceMiss(inout radiance_ray_payload payload)
{
	payload.color = sky.SampleLevel(wrapSampler, WorldRayDirection(), 0).xyz;
}

[shader("anyhit")]
void radianceAnyHit(inout radiance_ray_payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
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
	AcceptHitAndEndSearch(); // Actually redundant with the flag we specified in the TraceRay call.
}


