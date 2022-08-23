#pragma once

#include "core/math_simd.h"

template <typename simd_t>
struct wN_bounding_sphere
{
	wN_vec3<simd_t> center;
	simd_t radius;
};

template <typename simd_t>
struct wN_bounding_capsule
{
	wN_vec3<simd_t> positionA;
	wN_vec3<simd_t> positionB;
	simd_t radius;
};

template <typename simd_t>
struct wN_bounding_cylinder
{
	wN_vec3<simd_t> positionA;
	wN_vec3<simd_t> positionB;
	simd_t radius;
};

template <typename simd_t>
struct wN_bounding_box
{
	wN_vec3<simd_t> minCorner;
	wN_vec3<simd_t> maxCorner;
};

template <typename simd_t>
struct wN_line_segment
{
	wN_vec3<simd_t> a, b;
};

template <typename simd_t>
inline wN_vec3<simd_t> closestPoint_PointSegment(const wN_vec3<simd_t>& q, const wN_line_segment<simd_t>& l)
{
	wN_vec3<simd_t> ab = l.b - l.a;
	simd_t t = dot(q - l.a, ab) / squaredLength(ab);
	t = clamp01(t);
	return l.a + t * ab;
}

template <typename simd_t>
inline wN_vec3<simd_t> closestPoint_PointAABB(const wN_vec3<simd_t>& q, const wN_bounding_box<simd_t>& aabb)
{
	wN_vec3<simd_t> result;
	for (uint32 i = 0; i < 3; ++i)
	{
		simd_t v = q.data[i];
		v = ifThen(v < aabb.minCorner.data[i], aabb.minCorner.data[i], v);
		v = ifThen(v > aabb.maxCorner.data[i], aabb.maxCorner.data[i], v);
		result.data[i] = v;
	}
	return result;
}

