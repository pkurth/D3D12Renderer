#pragma once

#include "core/math_simd.h"

#ifndef BV_SIMD_WIDTH
#define BV_SIMD_WIDTH 4
#endif

#if BV_SIMD_WIDTH == 4
typedef w4_vec2 w_vec2;
typedef w4_vec3 w_vec3;
typedef w4_vec4 w_vec4;
typedef w4_quat w_quat;
typedef w4_mat2 w_mat2;
typedef w4_mat3 w_mat3;
typedef w4_float w_float;
typedef w4_int w_int;
#elif BV_SIMD_WIDTH == 8 && defined(SIMD_AVX_2)
typedef w8_vec2 w_vec2;
typedef w8_vec3 w_vec3;
typedef w8_vec4 w_vec4;
typedef w8_quat w_quat;
typedef w8_mat2 w_mat2;
typedef w8_mat3 w_mat3;
typedef w8_float w_float;
typedef w8_int w_int;
#endif



struct w_bounding_sphere
{
	w_vec3 center;
	w_float radius;
};

struct w_bounding_capsule
{
	w_vec3 positionA;
	w_vec3 positionB;
	w_float radius;
};

struct w_bounding_cylinder
{
	w_vec3 positionA;
	w_vec3 positionB;
	w_float radius;
};

struct w_bounding_box
{
	w_vec3 minCorner;
	w_vec3 maxCorner;
};

struct w_line_segment
{
	w_vec3 a, b;
};

inline w_vec3 closestPoint_PointSegment(const w_vec3& q, const w_line_segment& l)
{
	w_vec3 ab = l.b - l.a;
	w_float t = dot(q - l.a, ab) / squaredLength(ab);
	t = clamp01(t);
	return l.a + t * ab;
}

inline w_vec3 closestPoint_PointAABB(const w_vec3& q, const w_bounding_box& aabb)
{
	w_vec3 result;
	for (uint32 i = 0; i < 3; ++i)
	{
		w_float v = q.data[i];
		v = ifThen(v < aabb.minCorner.data[i], aabb.minCorner.data[i], v);
		v = ifThen(v > aabb.maxCorner.data[i], aabb.maxCorner.data[i], v);
		result.data[i] = v;
	}
	return result;
}

