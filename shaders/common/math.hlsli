#ifndef MATH_H
#define MATH_H

static const float pi = 3.14159265359f;
static const float invPI = 0.31830988618379067153776752674503f;
static const float inv2PI = 0.15915494309189533576888376337251f;
static const float2 invAtan = float2(inv2PI, invPI);


static float inverseLerp(float l, float u, float v) { return (v - l) / (u - l); }
static float remap(float v, float oldL, float oldU, float newL, float newU) { return lerp(newL, newU, inverseLerp(oldL, oldU, v)); }

static float solidAngleOfSphere(float radius, float distance)
{
	// The angular radius of a sphere is p = arcsin(radius / d). 
	// The solid angle of a circular cap (projection of sphere) is 2pi * (1 - cos(p)).
	// cos(arcsin(x)) = sqrt(1 - x*x)

	float s = radius / distance;
	return 2.f * pi * (1.f - sqrt(max(0.f, 1.f - s * s)));
}

static uint flatten2D(uint2 coord, uint2 dim)
{
	return coord.x + coord.y * dim.x;
}

static uint flatten2D(uint2 coord, uint width)
{
	return coord.x + coord.y * width;
}

// Flattened array index to 2D array index.
static uint2 unflatten2D(uint idx, uint2 dim)
{
	return uint2(idx % dim.x, idx / dim.x);
}

inline bool isSaturated(float a) { return a == saturate(a); }
inline bool isSaturated(float2 a) { return isSaturated(a.x) && isSaturated(a.y); }
inline bool isSaturated(float3 a) { return isSaturated(a.x) && isSaturated(a.y) && isSaturated(a.z); }
inline bool isSaturated(float4 a) { return isSaturated(a.x) && isSaturated(a.y) && isSaturated(a.z) && isSaturated(a.w); }

#endif
