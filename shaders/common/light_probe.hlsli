#ifndef LIGHT_PROBE_HLSLI
#define LIGHT_PROBE_HLSLI

#ifdef HLSL
#include "math.hlsli"
#endif

#define LIGHT_PROBE_RESOLUTION	6
#define LIGHT_PROBE_TOTAL_RESOLUTION (LIGHT_PROBE_RESOLUTION + 2)

#define LIGHT_PROBE_DEPTH_RESOLUTION	14
#define LIGHT_PROBE_TOTAL_DEPTH_RESOLUTION (LIGHT_PROBE_DEPTH_RESOLUTION + 2)

#define NUM_RAYS_PER_PROBE 64

#define ENERGY_CONSERVATION 0.95f





static float signNotZero(float v)
{
	return (v >= 0.f) ? 1.f : -1.f;
}

static vec2 signNotZero(vec2 v)
{
	return vec2(signNotZero(v.x), signNotZero(v.y));
}

#ifdef HLSL
#define YX(vec) vec.yx
#else
#define YX(vec) vec2(vec.y, vec.x)
#endif


// [A Survey of Efficient Representations for Independent Unit Vectors]
// Maps between 3D direction and encoded vec2 [-1, 1]^2.
static vec2 encodeOctahedral(vec3 dir)
{
	float l1Norm = abs(dir.x) + abs(dir.y) + abs(dir.z);
	vec2 result = (1.f / l1Norm) * dir.xy;

	if (dir.z < 0.f)
	{
		result = (1.f - abs(YX(result))) * signNotZero(result);
	}

	return result;
}

static vec3 decodeOctahedral(vec2 o)
{
	vec3 v = vec3(o, 1.f - abs(o.x) - abs(o.y));
	if (v.z < 0.f)
	{
		v.xy = (1.f - abs(YX(v))) * signNotZero(v.xy);
	}
	return normalize(v);
}

static vec3 linearIndexTo3DIndex(uint32 i, uint32 countX, uint32 countY)
{
	uint32 slice = countX * countY;
	uint32 z = i / slice;
	uint32 xy = i % slice;
	uint32 y = xy / countX;
	uint32 x = xy % countX;

	return vec3((float)x, (float)y, (float)z);
}

struct light_probe_grid_cb
{
	vec3 minCorner;
	float cellSize;
	uint32 countX;
	uint32 countY;
	uint32 countZ;

	uint32 padding;

	vec3 linearIndexTo3DIndex(uint32 i)
	{
		return ::linearIndexTo3DIndex(i, countX, countY);
	}

	vec3 indexToPosition(vec3 i)
	{
		return i * cellSize + minCorner;
	}

	vec3 linearIndexToPosition(uint32 i)
	{
		return indexToPosition(linearIndexTo3DIndex(i));
	}

#ifdef HLSL
	int3 baseGridCoordinate(float3 position)
	{
		return clamp(int3((position - minCorner) / cellSize), int3(0, 0, 0), int3(countX, countY, countZ) - 1);
	}

	float2 directionToUV(float3 dir, float scale)
	{
		float2 oct = encodeOctahedral(dir);
		oct *= scale;
		oct = oct * 0.5f + 0.5f;
		return oct;
	}

	float3 sampleIrradianceAtPosition(float3 position, float3 normal, Texture2D<float3> irradiance, Texture2D<float2> depth, SamplerState linearSampler)
	{
		int3 baseIndex = baseGridCoordinate(position);
		float3 basePosition = (float3)baseIndex * cellSize + minCorner;

		float3 irradianceSum = 0.f;
		float weightSum = 0.f;

		float3 barycentric =  saturate((position - basePosition) / cellSize);

		const float irradianceOctScale = (float)LIGHT_PROBE_RESOLUTION / (float)LIGHT_PROBE_TOTAL_RESOLUTION;
		const float depthOctScale = (float)LIGHT_PROBE_DEPTH_RESOLUTION / (float)LIGHT_PROBE_TOTAL_DEPTH_RESOLUTION;

		float2 irradianceOct = directionToUV(normal, irradianceOctScale);


		float2 uvScale = 1.f / float2(countX * countY, countZ);

		const float normalBias = 0.0001f;

		for (int i = 0; i < 8; ++i)
		{
			int3 offset = int3(i, i >> 1, i >> 2) & 0x1;
			int3 probeIndex3 = clamp(baseIndex + offset, int3(0, 0, 0), int3(countX, countY, countZ) - 1);
			float2 uvOffset = float2(probeIndex3.y * countX + probeIndex3.x, probeIndex3.z);

			float weight = 1.f;


			float3 probePosition = indexToPosition((float3)probeIndex3);
			float3 pointToProbe = probePosition - position;

			// Don't sample probes which are behind us.
			weight *= square(max(0.0001f, (dot(normalize(pointToProbe), normal) + 1.f) * 0.5f)) + 0.2f;


			// Moment visibility.
#if 0
			pointToProbe -= normal * normalBias; // Offset a bit to avoid sampling directly at the surface. This shortens the vector.

			float distToProbe = length(pointToProbe);
			pointToProbe *= (1.f / distToProbe); // Normalize.

			float2 depthUV = (uvOffset + directionToUV(-pointToProbe, depthOctScale));
			float2 meanAndVariance = depth.SampleLevel(linearSampler, depthUV, 0);

			float mean = meanAndVariance.x;
			float variance = abs(square(meanAndVariance.x) - meanAndVariance.y);

			float chebyshevWeight = variance / (variance + square(max(distToProbe - mean, 0.f)));

			chebyshevWeight = max(pow3(chebyshevWeight), 0.f); // Increase contrast in the weight.
			weight *= (distToProbe <= mean) ? 1.f : chebyshevWeight;
#endif


			
			// Trilinear weights.
			float3 trilinear = lerp(1.f - barycentric, barycentric, offset);
			weight *= trilinear.x * trilinear.y * trilinear.z;


			float2 irradianceUV = (uvOffset + irradianceOct) * uvScale;
			irradianceSum += irradiance.SampleLevel(linearSampler, irradianceUV, 0) * weight;
			weightSum += weight;
		}

		return irradianceSum * (1.f / max(weightSum, 1e-5f)) * ENERGY_CONSERVATION;
	}
#endif

};


#endif
