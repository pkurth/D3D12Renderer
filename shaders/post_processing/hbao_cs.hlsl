#include "cs.hlsli"
#include "post_processing_rs.hlsli"
#include "camera.hlsli"
#include "random.hlsli"

// This file is based on https://github.com/scanberg/hbao

ConstantBuffer<hbao_cb> cb				: register(b0);
ConstantBuffer<camera_cb> camera		: register(b1);

Texture2D<float> depthBuffer			: register(t0);

RWTexture2D<float> resultTexture		: register(u0);

SamplerState linearSampler				: register(s0);

static float3 positionAt(float2 uv)
{
	float depth = depthBuffer.SampleLevel(linearSampler, uv, cb.depthBufferMipLevel);
	float3 pos = camera.restoreViewSpacePositionEyeDepth(uv, depth);
	return pos;
}

static float length2(float3 v)
{
	return dot(v, v);
}

static float3 minDiff(float3 P, float3 right, float3 left)
{
	float3 V1 = right - P;
	float3 V2 = P - left;
	return (length2(V1) < length2(V2)) ? V1 : V2;
}

static float2 rotateDirections(float2 dir, float2 cosSin)
{
	return float2(dir.x * cosSin.x - dir.y * cosSin.y,
		dir.x * cosSin.y + dir.y * cosSin.x);
}

static float2 snapUVOffset(float2 uv, float2 dims, float2 invDims)
{
	return round(uv * dims) * invDims;
}

static float tanToSin(float x)
{
	return x * rsqrt(x * x + 1.f);
}

static float invLength(float2 v)
{
	return rsqrt(dot(v, v));
}

static const float tanBias = tan(30.f * M_PI / 180.f);
static float biasedTangent(float3 v)
{
	return v.z * invLength(v.xy) + tanBias;
}

static float tangent(float3 P, float3 S)
{
	return -(P.z - S.z) * invLength(S.xy - P.xy);
}

static float falloff(float d2, float negInvRadius2)
{
	return d2 * negInvRadius2 + 1.f;
}

static float horizonOcclusion(float2 centerUV, float2 deltaUV, float3 P, 
	float3 dPdu, float3 dPdv, float jitter, float numSamples,
	float radius2, float negInvRadius2,
	float2 dims, float2 invDims)
{
	// Offset the first coord with some noise.
	float2 uv = centerUV + snapUVOffset(jitter * deltaUV, dims, invDims);
	deltaUV = snapUVOffset(deltaUV, dims, invDims);

	// Calculate the tangent vector.
	float3 T = deltaUV.x * dPdu + deltaUV.y * dPdv;

	// Get the angle of the tangent vector from the viewspace axis.
	float tanH = biasedTangent(T);
	float sinH = tanToSin(tanH);

	float ao = 0.f;

	// Sample to find the maximum angle
	for (float s = 0; s < numSamples; ++s)
	{
		uv += deltaUV;
		float3 S = positionAt(uv);
		float tanS = tangent(P, S);
		float d2 = length2(S - P);

		// Is the sample within the radius and the angle greater?
		if (d2 < radius2 && tanS > tanH)
		{
			float sinS = tanToSin(tanS);
			// Apply falloff based on the distance.
			ao += falloff(d2, negInvRadius2) * (sinS - sinH);

			tanH = tanS;
			sinH = sinS;
		}
	}

	return ao;
}

[RootSignature(HBAO_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(cs_input IN)
{
	if (IN.dispatchThreadID.x >= cb.screenWidth || IN.dispatchThreadID.y >= cb.screenHeight) 
	{
		return;
	}

	float2 screenDims = float2(cb.screenWidth, cb.screenHeight);
	float2 invScreenDims = rcp(screenDims);

	float2 centerUV = (IN.dispatchThreadID.xy + float2(0.5f, 0.5f)) * invScreenDims;
	float3 centerPos = positionAt(centerUV);

	if (centerPos.z < -1000.f)
	{
		resultTexture[IN.dispatchThreadID.xy] = 0.f;
		return;
	}

	float3 rightPos = positionAt(centerUV + float2(invScreenDims.x, 0.f));
	float3 leftPos = positionAt(centerUV - float2(invScreenDims.x, 0.f));
	float3 topPos = positionAt(centerUV - float2(0.f, invScreenDims.y));
	float3 bottomPos = positionAt(centerUV + float2(0.f, invScreenDims.y));

	float3 dPdu = minDiff(centerPos, rightPos, leftPos);
	float3 dPdv = minDiff(centerPos, bottomPos, topPos) * (screenDims.y * invScreenDims.x);

	float2 rayRadiusUV = 0.5f * cb.radius * camera.proj._m00_m11 / -centerPos.z;

	float rayRadiusPix = rayRadiusUV.x * screenDims.x;


	float ao = 1.f;

	if (rayRadiusPix > 1.f)
	{
		ao = 0.f;

		float jitter = random(centerUV * 9523.f + cb.seed) * 0.4f;

		// Compute steps.
		float numSteps = min((float)cb.maxNumStepsPerRay, rayRadiusPix);

		// Divide by Ns+1 so that the farthest samples are not fully attenuated.
		float stepSizePix = rayRadiusPix / (numSteps + 1);

		// Clamp numSteps if it is greater than the max kernel footprint.
		float maxNumSteps = cb.maxNumStepsPerRay / stepSizePix;
		if (maxNumSteps < numSteps)
		{
			// Use dithering to avoid AO discontinuities.
			numSteps = floor(maxNumSteps + jitter);
			numSteps = max(numSteps, 1);
			stepSizePix = cb.maxNumStepsPerRay / numSteps;
		}

		float2 stepSizeUV = stepSizePix * invScreenDims;


		float angle = random(centerUV * 5416.f + cb.seed) * M_PI * 2.f;
		float2 randomRotation = float2(cos(angle), sin(angle)); // TODO: Maybe get this from texture.

		// Apply noise to initial rotation.
		float2 dir = rotateDirections(float2(1.f, 0.f), randomRotation);

		float2 rayDeltaRotation = cb.rayDeltaRotation;

		float radius2 = cb.radius * cb.radius;
		float negInvRadius2 = rcp(radius2);

		for (float d = 0; d < (float)cb.numRays; d += 1.f)
		{
			float2 deltaUV = dir * stepSizeUV;

			// Sample the pixels along the direction.
			ao += horizonOcclusion(
				centerUV, deltaUV,
				centerPos,
				dPdu, dPdv,
				jitter, numSteps,
				radius2, negInvRadius2,
				screenDims, invScreenDims);

			dir = rotateDirections(dir, rayDeltaRotation);
		}

		// Average the results and produce the final AO.
		ao = saturate(1.f - ao / cb.numRays * cb.strength);
	}

	resultTexture[IN.dispatchThreadID.xy] = ao;
}
