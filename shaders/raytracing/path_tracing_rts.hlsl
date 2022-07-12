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

		float2 rnd = float2(2.f * M_PI * nextRand(randSeed), constants.lensRadius * nextRand(randSeed));
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

static float probabilityToSampleDiffuse(float roughness)
{
	// I don't know what a good way to choose is. My understanding is that it doesn't really matter, as long as you account for the probability in your results. 
	// It has an impact on covergence speed though.
	// TODO: Can we calculate this using fresnel?
	return 0.5f;
	return roughness;
}

static float3 calculateIndirectLighting(inout uint randSeed, surface_info surface, uint recursion)
{
	float probDiffuse = probabilityToSampleDiffuse(surface.roughness);
	float chooseDiffuse = (nextRand(randSeed) < probDiffuse);

	if (chooseDiffuse)
	{
		float3 L = getCosHemisphereSample(randSeed, surface.N);
		float3 bounceColor = traceRadianceRay(surface.P, L, randSeed, recursion);

		// Accumulate the color: (NdotL * incomingLight * dif / pi) 
		// Probability of sampling:  (NdotL / pi) * probDiffuse
		return bounceColor * surface.albedo.rgb / probDiffuse;
	}
	else
	{
		float3 H = importanceSampleGGX(randSeed, surface.N, surface.roughness);
		float3 L = reflect(-surface.V, H);

		float3 bounceColor = traceRadianceRay(surface.P, L, randSeed, recursion);

		float NdotV = saturate(dot(surface.N, surface.V));
		float NdotL = saturate(dot(surface.N, L));
		float NdotH = saturate(dot(surface.N, H));
		float LdotH = saturate(dot(L, H));

		float D = distributionGGX(NdotH, surface.roughness);
		float G = geometrySmith(NdotL, NdotV, surface.roughness);
		float3 F = fresnelSchlick(LdotH, surface.F0);

		float3 numerator = D * G * F;
		float denominator = 4.f * NdotV * NdotL;
		float3 brdf = numerator / max(denominator, 0.001f);

		// Probability of sampling vector H from GGX.
		float ggxProb = max(D * NdotH / (4.f * LdotH), 0.01f);

		// Accumulate the color:  ggx-BRDF * incomingLight * NdotL / probability-of-sampling
		//    -> Should really simplify the math above.
		return NdotL * bounceColor * brdf / (ggxProb * (1.f - probDiffuse));
	}
}


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
	surface.N = normalize(transformDirectionToWorld(interpolateAttribute(normals, attribs)));
	surface.V = -WorldRayDirection();
	surface.P = hitWorldPosition();

	surface.emission = (float3)0.f;
	surface.albedo = (float4)1.f;
	surface.roughness = 1.f;
	surface.metallic = 0.f;

	if (constants.useRealMaterials)
	{
		uint mipLevel = 0;

		surface.albedo = (((flags & MATERIAL_USE_ALBEDO_TEXTURE)
			? albedoTex.SampleLevel(wrapSampler, uv, mipLevel)
			: float4(1.f, 1.f, 1.f, 1.f))
			* unpackColor(material.albedoTint));

		// We ignore normal maps for now.

		surface.roughness = (flags & MATERIAL_USE_ROUGHNESS_TEXTURE)
			? roughTex.SampleLevel(wrapSampler, uv, mipLevel)
			: material.getRoughnessOverride();

		surface.metallic = (flags & MATERIAL_USE_METALLIC_TEXTURE)
			? metalTex.SampleLevel(wrapSampler, uv, mipLevel)
			: material.getMetallicOverride();

		surface.emission = material.emission;
	}

	surface.roughness = clamp(surface.roughness, 0.01f, 0.99f);

	surface.inferRemainingProperties();

	payload.color = surface.emission;
	payload.color += calculateIndirectLighting(payload.randSeed, surface, payload.recursion);


	if (constants.enableDirectLighting)
	{
		// Sun light.
		{
			float3 sunL = -normalize(float3(-0.6f, -1.f, -0.3f));
			float3 sunRadiance = float3(1.f, 0.93f, 0.76f) * constants.lightIntensityScale * 2.f;

			light_info light;
			light.initialize(surface, sunL, sunRadiance);

			payload.color +=
				calculateDirectLighting(surface, light).evaluate(surface.albedo)
				* traceShadowRay(surface.P, sunL, 10000.f, payload.recursion);
		}



		// Random point light.
		{
			uint lightIndex = min(uint(NUM_LIGHTS * nextRand(payload.randSeed)), NUM_LIGHTS - 1);

			light_info light;
			light.initializeFromRandomPointOnSphereLight(surface, pointLights[lightIndex], constants.pointLightRadius, payload.randSeed);

			float pointLightVisibility = traceShadowRay(surface.P, light.L, light.distanceToLight, payload.recursion);

			float3 pointLightColor =
				calculateDirectLighting(surface, light).evaluate(surface.albedo)
				* pointLightVisibility;

			float pointLightSolidAngle = solidAngleOfSphere(constants.pointLightRadius, light.distanceToLight) * 0.5f; // Divide by 2, since we are only interested in hemisphere.
			if (constants.multipleImportanceSampling)
			{
				// Multiple importance sampling. At least if I've done this correctly. See http://www.cs.uu.nl/docs/vakken/magr/2015-2016/slides/lecture%2008%20-%20variance%20reduction.pdf, slide 50.
				float lightPDF = 1.f / (pointLightSolidAngle * NUM_LIGHTS); // Correct for PDFs, see comment below.

				// Lambertian part.
				float diffusePDF = dot(surface.N, light.L) * M_INV_PI; // Cosine-distributed for Lambertian BRDF.

				// Specular part.
				float D = distributionGGX(surface, light);
				float specularPDF = max(D * light.NdotH / (4.f * light.LdotH), 0.01f);

				float probDiffuse = probabilityToSampleDiffuse(surface.roughness);

				// Total BRDF PDF. This is the probability that we had randomly hit this direction using our brdf importance sampling.
				float brdfPDF = lerp(specularPDF, diffusePDF, probDiffuse);

				// Blend PDFs.
				float t = lightPDF / (lightPDF + brdfPDF);
				float misPDF = lerp(brdfPDF, lightPDF, t); // Balance heuristic.

				pointLightColor /= misPDF;
			}
			else
			{
				pointLightColor = pointLightColor
					* NUM_LIGHTS			 // Correct for probability of choosing this particular light.
					* pointLightSolidAngle;  // Correct for probability of "randomly" hitting this light. I *think* this is correct. See https://github.com/NVIDIA/Q2RTX/blob/master/src/refresh/vkpt/shader/light_lists.h#L295.
			}

			payload.color += pointLightColor;
		}
	}
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


