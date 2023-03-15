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


inline float asfloat(uint32 u) { return *(float*)&u; }
inline uint32 asuint(float f) { return *(uint32*)&f; }

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
static float random1(float x) { return floatConstruct(hash(asuint(x))); }
static float random1(vec2 v)  { return floatConstruct(hash(asuint(v.x), asuint(v.y))); }
static float random1(vec3 v)  { return floatConstruct(hash(asuint(v.x), asuint(v.y), asuint(v.z))); }
static float random1(vec4 v)  { return floatConstruct(hash(asuint(v.x), asuint(v.y), asuint(v.z), asuint(v.w))); }

static vec2 random2(vec2 v) { return vec2(floatConstruct(hash(asuint(v.x * 15123.6989f))), floatConstruct(hash(asuint(v.y * 6192.234f)))); }
static vec3 random3(vec3 v) { return vec3(floatConstruct(hash(asuint(v.x * 15123.6989f))), floatConstruct(hash(asuint(v.y * 6192.234f))), floatConstruct(hash(asuint(v.z * 31923.123f)))); }

static vec3 valueNoise(vec2 x)
{
	vec2 p = floor(x);
	vec2 w = frac(x);

	vec2 u = w * w * w * (w * (w * 6.f - 15.f) + 10.f);
	vec2 du = 30.f * w * w * (w * (w - 2.f) + 1.f);

	float a = random1(p);
	float b = random1(p + vec2(1, 0));
	float c = random1(p + vec2(0, 1));
	float d = random1(p + vec2(1, 1));

	float k0 = a;
	float k1 = b - a;
	float k2 = c - a;
	float k3 = a - b - c + d;

	float value = -1.f + 2.f * (k0 + k1 * u.x + k2 * u.y + k3 * u.x * u.y);
	vec2 deriv = 2.f * du *
		vec2(
			k1 + k3 * u.y,
			k2 + k3 * u.x);

	return vec3(value, deriv.x, deriv.y);
}

static vec4 valueNoise(vec3 x)
{
	vec3 p = floor(x);
	vec3 w = frac(x);

	vec3 u = w * w * w * (w * (w * 6.f - 15.f) + 10.f);
	vec3 du = 30.f * w * w * (w * (w - 2.f) + 1.f);

	float a = random1(p + vec3(0, 0, 0));
	float b = random1(p + vec3(1, 0, 0));
	float c = random1(p + vec3(0, 1, 0));
	float d = random1(p + vec3(1, 1, 0));
	float e = random1(p + vec3(0, 0, 1));
	float f = random1(p + vec3(1, 0, 1));
	float g = random1(p + vec3(0, 1, 1));
	float h = random1(p + vec3(1, 1, 1));

	float k0 = a;
	float k1 = b - a;
	float k2 = c - a;
	float k3 = e - a;
	float k4 = a - b - c + d;
	float k5 = a - c - e + g;
	float k6 = a - b - e + f;
	float k7 = -a + b + c - d + e - f - g + h;

	float value = -1.f + 2.f * (k0 + k1 * u.x + k2 * u.y + k3 * u.z + k4 * u.x * u.y + k5 * u.y * u.z + k6 * u.z * u.x + k7 * u.x * u.y * u.z);
	vec3 deriv = 2.f * du * 
		vec3(
			k1 + k4 * u.y + k6 * u.z + k7 * u.y * u.z,
			k2 + k5 * u.z + k4 * u.x + k7 * u.z * u.x,
			k3 + k6 * u.x + k5 * u.y + k7 * u.x * u.y);

	return vec4(value, deriv.x, deriv.y, deriv.z);
}

static vec3 gradientNoise(vec2 x)
{
	vec2 p = floor(x);
	vec2 w = frac(x);

	vec2 u = w * w * w * (w * (w * 6.f - 15.f) + 10.f);
	vec2 du = 30.f * w * w * (w * (w - 2.f) + 1.f);

	// Gradients
	vec2 ga = random2(p);
	vec2 gb = random2(p + vec2(1, 0));
	vec2 gc = random2(p + vec2(0, 1));
	vec2 gd = random2(p + vec2(1, 1));

	// Projections
	float va = dot(ga, w);
	float vb = dot(gb, w - vec2(1, 0));
	float vc = dot(gc, w - vec2(0, 1));
	float vd = dot(gd, w - vec2(1, 1));

	// Interpolation
	float v = va +
		u.x * (vb - va) +
		u.y * (vc - va) +
		u.x * u.y * (va - vb - vc + vd);

	float k = (va - vb - vc + vd);

	vec2 d = ga +
		u.x * (gb - ga) +
		u.y * (gc - ga) +
		u.x * u.y * (ga - gb - gc + gd) +

		du * vec2(
			vb - va + u.y * k,
			vc - va + u.x * k
		);

	return vec3(v, d.x, d.y);
}

static vec4 gradientNoise(vec3 x)
{
	vec3 p = floor(x);
	vec3 w = frac(x);

	vec3 u = w * w * w * (w * (w * 6.f - 15.f) + 10.f);
	vec3 du = 30.f * w * w * (w * (w - 2.f) + 1.f);

	// Gradients
	vec3 ga = random3(p + vec3(0, 0, 0));
	vec3 gb = random3(p + vec3(1, 0, 0));
	vec3 gc = random3(p + vec3(0, 1, 0));
	vec3 gd = random3(p + vec3(1, 1, 0));
	vec3 ge = random3(p + vec3(0, 0, 1));
	vec3 gf = random3(p + vec3(1, 0, 1));
	vec3 gg = random3(p + vec3(0, 1, 1));
	vec3 gh = random3(p + vec3(1, 1, 1));

	// Projections
	float va = dot(ga, w - vec3(0, 0, 0));
	float vb = dot(gb, w - vec3(1, 0, 0));
	float vc = dot(gc, w - vec3(0, 1, 0));
	float vd = dot(gd, w - vec3(1, 1, 0));
	float ve = dot(ge, w - vec3(0, 0, 1));
	float vf = dot(gf, w - vec3(1, 0, 1));
	float vg = dot(gg, w - vec3(0, 1, 1));
	float vh = dot(gh, w - vec3(1, 1, 1));

	// Interpolation
	float v = va +
		u.x * (vb - va) +
		u.y * (vc - va) +
		u.z * (ve - va) +
		u.x * u.y * (va - vb - vc + vd) +
		u.y * u.z * (va - vc - ve + vg) +
		u.z * u.x * (va - vb - ve + vf) +
		u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);

	vec3 yzx(u.y, u.z, u.x);
	vec3 zxy(u.z, u.x, u.y);

	vec3 d = ga +
		u.x * (gb - ga) +
		u.y * (gc - ga) +
		u.z * (ge - ga) +
		u.x * u.y * (ga - gb - gc + gd) +
		u.y * u.z * (ga - gc - ge + gg) +
		u.z * u.x * (ga - gb - ge + gf) +
		u.x * u.y * u.z * (-ga + gb + gc - gd + ge - gf - gg + gh) +

		du * (vec3(vb - va, vc - va, ve - va) +
			yzx * vec3(va - vb - vc + vd, va - vc - ve + vg, va - vb - ve + vf) +
			zxy * vec3(va - vb - ve + vf, va - vb - vc + vd, va - vc - ve + vg) +
			yzx * zxy * (-va + vb + vc - vd + ve - vf - vg + vh));

	return vec4(v, d.x, d.y, d.z);
}

typedef vec3(*fbm_noise_2D)(vec2);
typedef vec4(*fbm_noise_3D)(vec3);

static vec3 fbm(fbm_noise_2D noiseFunc, vec2 x, uint32 numOctaves = 6, float lacunarity = 1.98f, float gain = 0.49f)
{
	float value = 0.f;
	float amplitude = 0.5f;

	vec2 deriv(0.f);
	float m = 1.f;

	for (uint32 i = 0; i < numOctaves; ++i)
	{
		vec3 n = noiseFunc(x);

		ASSERT(n.x <= 1.f);
		ASSERT(n.x >= -1.f);

		value += amplitude * n.x;		// Accumulate values.
		deriv += amplitude * m * n.yz;  // Accumulate derivatives.

		amplitude *= gain;

		x *= lacunarity;
		m *= lacunarity;
	}
	return vec3(value, deriv.x, deriv.y);
}

static vec4 fbm(fbm_noise_3D noiseFunc, vec3 x, uint32 numOctaves = 6, float lacunarity = 1.98f, float gain = 0.49f)
{
	float value = 0.f;
	float amplitude = 0.5;

	vec3 deriv(0.f);
	float m = 1.f;

	for (uint32 i = 0; i < numOctaves; ++i)
	{
		vec4 n = noiseFunc(x);

		value += amplitude * n.x;		// Accumulate values.
		deriv += amplitude * m * n.yzw; // Accumulate derivatives.

		amplitude *= gain;

		x *= lacunarity;
		m *= lacunarity;
	}
	return vec4(value, deriv.x, deriv.y, deriv.z);
}

