#include "cs.hlsli"
#include "camera.hlsli"
#include "lighting.hlsli"

#define RS \
    "RootFlags(0), " \
    "CBV(b0), " \
    "CBV(b1), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_BORDER," \
        "addressV = TEXTURE_ADDRESS_BORDER," \
        "addressW = TEXTURE_ADDRESS_BORDER," \
        "filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT)," \
    "DescriptorTable( SRV(t0, numDescriptors = 2, flags = DESCRIPTORS_VOLATILE), UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )"

#define BLOCK_SIZE 16

ConstantBuffer<camera_cb> camera			: register(b0);
ConstantBuffer<directional_light_cb> sun	: register(b1);
Texture2D<float> depthBuffer				: register(t0);
Texture2D<float> shadowMap					: register(t1);

RWTexture2D<float4> output					: register(u0);

SamplerComparisonState shadowSampler		: register(s0);


// Returns (distanceToIntersection, distanceThroughVolume)
static float2 raySphereIntersection(float3 origin, float3 direction, float3 center, float radius)
{
	float3 oc = origin - center;
	const float a = 1.f; // dot(d, d)
	float b = 2.f * dot(oc, direction);
	float c = dot(oc, oc) - radius * radius;
	float d = b * b - 4.f * a * c;

	float2 result = float2(-1.f, -1.f);
	if (d > 0.f)
	{
		float s = sqrt(d);
		float distToNear = max(0.f, (-b - s) / (2.f * a));
		float distToFar = (-b + s) / (2.f * a);

		if (distToFar >= 0.f)
		{
			result = float2(distToNear, distToFar - distToNear);
		}
	}
	return result;
}

// Returns (distanceToIntersection, distanceThroughVolume)
static float2 rayBoxIntersection(float3 origin, float3 direction, float3 minCorner, float3 maxCorner)
{
	float3 invDir = 1.f / direction; // This can be Inf (when one direction component is 0) but still works.

	float tx1 = (minCorner.x - origin.x) * invDir.x;
	float tx2 = (maxCorner.x - origin.x) * invDir.x;

	float tmin = min(tx1, tx2);
	float tmax = max(tx1, tx2);

	float ty1 = (minCorner.y - origin.y) * invDir.y;
	float ty2 = (maxCorner.y - origin.y) * invDir.y;

	tmin = max(tmin, min(ty1, ty2));
	tmax = min(tmax, max(ty1, ty2));

	float tz1 = (minCorner.z - origin.z) * invDir.z;
	float tz2 = (maxCorner.z - origin.z) * invDir.z;

	tmin = max(tmin, min(tz1, tz2));
	tmax = min(tmax, max(tz1, tz2));

	float2 result = float2(-1.f, -1.f);
	if (tmax >= 0.f)
	{
		result = float2(max(0.f, tmin), tmax - tmin);
	}
	return result;
}

static float densityAtPoint(float3 position)
{
	return 50.8f;
}

static float opticalDepth(float3 position, float3 direction, float dist)
{
	return densityAtPoint(position) * dist;
}

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	const uint numInScatteringPoints = 30;

	const float3 minCorner = float3(-80.f, 0.f, -80.f);
	const float3 maxCorner = float3(80.f, 50.f, 80.f);

	const float3 waveLengths = float3(700, 530, 440);
	const float scatteringStrength = 1.f;
	// https://www.desmos.com/calculator/64odtmkk9m
	const float3 scatteringCoefficients = pow(10.f / waveLengths, 4) * scatteringStrength; // Rayleigh scattering.

	const float ditherPattern[4][4] = 
	{ 
		{ 0.0f, 0.5f, 0.125f, 0.625f},
		{ 0.75f, 0.22f, 0.875f, 0.375f},
		{ 0.1875f, 0.6875f, 0.0625f, 0.5625},
		{ 0.9375f, 0.4375f, 0.8125f, 0.3125} 
	};


	uint2 texCoord = IN.dispatchThreadID.xy;
	if (texCoord.x >= (uint)camera.screenDims.x || texCoord.y >= (uint)camera.screenDims.y)
	{
		return;
	}

	float depth = depthBuffer[texCoord];

	float2 screenUV = texCoord * camera.invScreenDims;

	float3 P = camera.restoreWorldSpacePosition(screenUV, depth);
	float3 O = camera.position.xyz;
	float3 V = P - O;
	float distanceToGeometry = length(V);
	V /= distanceToGeometry;

	float2 hitInfo = rayBoxIntersection(O, V, minCorner, maxCorner);

	float distToVolume = hitInfo.x;
	float distThroughVolume = min(hitInfo.y, distanceToGeometry);

	if (distToVolume > distanceToGeometry || distThroughVolume <= 0.f)
	{
		output[texCoord] = float4(0.f, 0.f, 0.f, 0.f); // TODO
		return;
	}

	float ditherOffset = ditherPattern[texCoord.x % 4][texCoord.y % 4];

	const float epsilon = 0.00001f;
	distToVolume += epsilon;
	distThroughVolume -= 2.f * epsilon;

	float stepSize = distThroughVolume / (numInScatteringPoints - 1);
	float3 inScatterPoint = O + (distToVolume + ditherOffset * stepSize) * V;
	float3 inScatteredLight = 0.f;

	float3 L = -sun.direction;

	for (uint i = 0; i < numInScatteringPoints; ++i)
	{
		float sunRayLength = rayBoxIntersection(inScatterPoint, L, minCorner, maxCorner).y;
		float sunRayOpticalDepth = opticalDepth(inScatterPoint, L, sunRayLength);
		float viewRayOpticalDepth = opticalDepth(inScatterPoint, -V, stepSize * i);

		float3 transmittance = exp(-(sunRayOpticalDepth + viewRayOpticalDepth) * scatteringCoefficients);
		float localDensity = densityAtPoint(inScatterPoint);

#if 1 // Cascaded vs single.
		float pixelDepth = dot(camera.forward.xyz, inScatterPoint - O);
#if 1 // Simple vs PCF
		float visibility = sampleCascadedShadowMapSimple(sun.viewProjs, inScatterPoint, 
			shadowMap, sun.viewports,
			shadowSampler, pixelDepth, sun.numShadowCascades,
			sun.cascadeDistances, sun.bias, sun.blendDistances);
#else
		float visibility = sampleCascadedShadowMapPCF(sun.vp, inScatterPoint, 
			shadowMap, sun.viewports,
			shadowSampler, sun.texelSize, pixelDepth, sun.numShadowCascades,
			sun.cascadeDistances, sun.bias, sun.blendArea);
#endif
#else
		uint currentCascadeIndex = sun.numShadowCascades - 1;
		float4 bias = sun.bias;
		float visibility = sampleShadowMapPCF(sun.viewProjs[currentCascadeIndex], inScatterPoint,
			shadowMap, sun.viewports[currentCascadeIndex],
			shadowSampler, sun.texelSize, bias[currentCascadeIndex]);
#endif

		inScatteredLight += localDensity * transmittance * visibility;
		inScatterPoint += V * stepSize;
	}

	inScatteredLight *= scatteringCoefficients * stepSize * sun.radiance;

	output[texCoord] = float4(inScatteredLight, 0.f);
}

