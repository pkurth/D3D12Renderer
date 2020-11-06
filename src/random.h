#pragma once

#include "math.h"

struct random_number_generator
{
	// XOR shift generator.

	uint32 state;

	inline uint32 randomUint()
	{
		uint32 x = state;
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		state = x;
		return x;
	}

	inline uint32 randomUintBetween(uint32 lo, uint32 hi)
	{
		return randomUint() % (hi - lo) + lo;
	}

	inline float randomFloat01()
	{
		return randomUint() / (float)UINT_MAX;
	}

	inline float randomFloatBetween(float lo, float hi)
	{
		return remap(randomFloat01(), 0.f, 1.f, lo, hi);
	}
};
