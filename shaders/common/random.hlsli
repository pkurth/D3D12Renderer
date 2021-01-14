#ifndef RANDOM_H
#define RANDOM_H

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


#endif
