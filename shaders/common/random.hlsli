#ifndef RANDOM_HLSLI
#define RANDOM_HLSLI

#include "math.hlsli"

static float halton(uint index, uint base)
{
	float fraction = 1.f;
	float result = 0.f;
	while (index > 0)
	{
		fraction /= (float)base;
		result += fraction * (index % base);
		index = ~~(index / base);
	}
	return result;
}

static float2 halton23(uint index)
{
	return float2(halton(index, 2), halton(index, 3));
}



static uint hash(uint x)
{
	x += (x << 10u);
	x ^= (x >> 6u);
	x += (x << 3u);
	x ^= (x >> 11u);
	x += (x << 15u);
	return x;
}

// Compound versions of the hashing algorithm I whipped together.
static uint hash(uint2 v) { return hash(v.x ^ hash(v.y)); }
static uint hash(uint3 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z)); }
static uint hash(uint4 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w)); }

// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
static float floatConstruct(uint m)
{
	const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
	const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

	m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
	m |= ieeeOne;                          // Add fractional part to 1.0

	float  f = asfloat(m);       // Range [1:2]
	return f - 1.f;              // Range [0:1]
}

// Pseudo-random value in half-open range [0:1].
static float random(float  x) { return floatConstruct(hash(asuint(x))); }
static float random(float2 v) { return floatConstruct(hash(asuint(v))); }
static float random(float3 v) { return floatConstruct(hash(asuint(v))); }
static float random(float4 v) { return floatConstruct(hash(asuint(v))); }




static float2 hammersley(uint i, uint N)
{
	// Van der Corpus sequence.
	uint bits = i;
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float vdc = float(bits) * 2.3283064365386963e-10; // / 0x100000000

	return float2(float(i) / float(N), vdc);
}

// "Next Generation Post Processing in Call of Duty: Advanced Warfare"
// http://advances.realtimerendering.com/s2014/index.html
static float interleavedGradientNoise(float2 uv, uint frameCount)
{
	frameCount &= 1023;
	const float2 magicFrameScale = float2(47.f, 17.f) * 0.695f;
	uv += frameCount * magicFrameScale;

	const float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	return frac(magic.z * frac(dot(uv, magic.xy)));
}





static uint initRand(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}
	return v0;
}

// Returns a pseudorandom float in [0..1].
static float nextRand(inout uint s)
{
	s = (1664525u * s + 1013904223u);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

// Returns a pseudorandom float in [lo..hi].
static float nextRandBetween(inout uint s, float lo, float hi)
{
	return nextRand(s) * (hi - lo) + lo;
}

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
static float3 getPerpendicularVector(float3 u)
{
	float3 a = abs(u);
	uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(u, float3(xm, ym, zm));
}

// Get a cosine-weighted random vector centered around a specified normal direction.
static float3 getCosHemisphereSample(inout uint randSeed, float3 hitNorm)
{
	// Get 2 random numbers to select our sample with.
	float2 randVal = float2(nextRand(randSeed), nextRand(randSeed));

	// Cosine weighted hemisphere sample from RNG.
	float3 bitangent = getPerpendicularVector(hitNorm);
	float3 tangent = cross(bitangent, hitNorm);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction.
	return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(1 - randVal.x);
}

static float3 getRandomPointOnUnitSphere(inout uint randSeed)
{
	float2 h = float2(nextRand(randSeed), nextRand(randSeed)) * float2(2.f, 2.f * M_PI) - float2(1.f, 0.f);
	float phi = h.y;
	return normalize(float3(sqrt(1.f - h.x * h.x) * float2(sin(phi), cos(phi)), h.x));
}

static float3 getRandomPointOnSphere(inout uint randSeed, float radius)
{
	return getRandomPointOnUnitSphere(randSeed) * radius;
}

static float2 getRandomPointOnUnitDisk(inout uint randSeed)
{
	float d;
	float2 result;

	do 
	{
		result = float2(nextRand(randSeed), nextRand(randSeed)) * 2.f - 1.f;
		d = dot(result, result);
	} while (d >= 1.f);

	return result;
}

static float2 getRandomPointOnDisk(inout uint randSeed, float radius)
{
	return getRandomPointOnUnitDisk(randSeed) * radius;
}



static float3 valueNoise(float2 x)
{
	float2 p = floor(x);
	float2 w = frac(x);

	float2 u = w * w * w * (w * (w * 6.f - 15.f) + 10.f);
	float2 du = 30.f * w * w * (w * (w - 2.f) + 1.f);

	float a = random(p);
	float b = random(p + float2(1, 0));
	float c = random(p + float2(0, 1));
	float d = random(p + float2(1, 1));

	float k0 = a;
	float k1 = b - a;
	float k2 = c - a;
	float k3 = a - b - c + d;

	float value = -1.f + 2.f * (k0 + k1 * u.x + k2 * u.y + k3 * u.x * u.y);
	float2 deriv = 2.f * du *
		float2(
			k1 + k3 * u.y,
			k2 + k3 * u.x);

	return float3(value, deriv.x, deriv.y);
}

static float3 fbm(float2 x, uint numOctaves = 6, float lacunarity = 1.98f, float gain = 0.49f)
{
	float value = 0.f;
	float amplitude = 0.5f;

	float2 deriv = 0.f;
	float m = 1.f;

	for (uint32 i = 0; i < numOctaves; ++i)
	{
		float3 n = valueNoise(x);

		value += amplitude * n.x;		// Accumulate values.
		deriv += amplitude * m * n.yz;  // Accumulate derivatives.

		amplitude *= gain;

		x *= lacunarity;
		m *= lacunarity;
	}
	return float3(value, deriv.x, deriv.y);
}





#endif
