#pragma once

#include "math.h"

struct random_number_generator
{
	// XOR shift generator.

	uint64 state;

	random_number_generator() {}
	random_number_generator(uint64 seed) { state = seed; }

	inline uint64 randomUint64()
	{
		// https://en.wikipedia.org/wiki/Xorshift

		uint64 x = state;
		x ^= x << 13;
		x ^= x >> 7;
		x ^= x << 17;
		state = x;
		return x;
	}

	inline uint32 randomUint32()
	{
		return (uint32)randomUint64();
	}

	inline uint64 randomUint64Between(uint64 lo, uint64 hi)
	{
		return randomUint64() % (hi - lo) + lo;
	}

	inline uint32 randomUint32Between(uint32 lo, uint32 hi)
	{
		return randomUint32() % (hi - lo) + lo;
	}

	inline float randomFloat01()
	{
		return randomUint32() / (float)UINT_MAX;
	}

	inline float randomFloatBetween(float lo, float hi)
	{
		return remap(randomFloat01(), 0.f, 1.f, lo, hi);
	}

	inline vec2 randomVec2Between(float lo, float hi)
	{
		return vec2(
			randomFloatBetween(lo, hi),
			randomFloatBetween(lo, hi));
	}

	inline vec3 randomVec3Between(float lo, float hi)
	{
		return vec3(
			randomFloatBetween(lo, hi),
			randomFloatBetween(lo, hi),
			randomFloatBetween(lo, hi));
	}

	inline vec4 randomVec4Between(float lo, float hi)
	{
		return vec4(
			randomFloatBetween(lo, hi),
			randomFloatBetween(lo, hi),
			randomFloatBetween(lo, hi),
			randomFloatBetween(lo, hi));
	}

	inline vec3 randomPointOnUnitSphere()
	{
		return normalize(randomVec3Between(-1.f, 1.f));
	}

	inline quat randomRotation(float maxAngle = M_PI)
	{
		return quat(randomPointOnUnitSphere(), randomFloatBetween(-maxAngle, maxAngle));
	}
};

static float halton(uint32 index, uint32 base)
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

static vec2 halton23(uint32 index)
{
	return vec2(halton(index, 2), halton(index, 3));
}



static uint32 hash(uint32 x)
{
	x += (x << 10u);
	x ^= (x >> 6u);
	x += (x << 3u);
	x ^= (x >> 11u);
	x += (x << 15u);
	return x;
}

// Compound versions of the hashing algorithm I whipped together.
static uint32 hash(uint32 x, uint32 y) { return hash(x ^ hash(y)); }
static uint32 hash(uint32 x, uint32 y, uint32 z) { return hash(x ^ hash(y) ^ hash(z)); }
static uint32 hash(uint32 x, uint32 y, uint32 z, uint32 w) { return hash(x ^ hash(y) ^ hash(z) ^ hash(w)); }


#define asfloat(u) (*(float*)&u)
#define asuint(f) (*(uint32*)&f)

// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
static float floatConstruct(uint32 m)
{
	const uint32 ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
	const uint32 ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

	m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
	m |= ieeeOne;                          // Add fractional part to 1.0

	float  f = asfloat(m);       // Range [1:2]
	return f - 1.f;              // Range [0:1]
}

// Pseudo-random value in half-open range [0:1].
static float random(float  x) { return floatConstruct(hash(asuint(x))); }
static float random(vec2 v) { return floatConstruct(hash(asuint(v.x), asuint(v.y))); }
static float random(vec3 v) { return floatConstruct(hash(asuint(v.x), asuint(v.y), asuint(v.z))); }
static float random(vec4 v) { return floatConstruct(hash(asuint(v.x), asuint(v.y), asuint(v.z), asuint(v.w))); }

// Based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
static float fbmNoise(vec2 st)
{
	vec2 i = floor(st);
	vec2 f = frac(st);

	// Four corners in 2D of a tile
	float a = random(i);
	float b = random(i + vec2(1.f, 0.f));
	float c = random(i + vec2(0.f, 1.f));
	float d = random(i + vec2(1.f, 1.f));

	vec2 u = f * f * (3.f - 2.f * f);

	return lerp(a, b, u.x) + lerp((c - a) * u.y, (d - b) * u.y, u.x);
}

static float fbm(vec2 st, uint32 numOctaves = 6, float lacunarity = 2.f, float gain = 0.5f)
{
	float value = 0.f;
	float amplitude = .5f;
	float frequency = 1.f;

	for (uint32 i = 0; i < numOctaves; ++i)
	{
		value += amplitude * fbmNoise(frequency * st);
		frequency *= lacunarity;
		amplitude *= gain;
	}
	return value;
}

