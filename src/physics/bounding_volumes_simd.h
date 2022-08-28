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

	static wN_bounding_box wN_bounding_box::fromMinMax(wN_vec3<simd_t> minCorner, wN_vec3<simd_t> maxCorner)
	{
		return wN_bounding_box{ minCorner, maxCorner };
	}

	static wN_bounding_box wN_bounding_box::fromCenterRadius(wN_vec3<simd_t> center, wN_vec3<simd_t> radius)
	{
		return wN_bounding_box{ center - radius, center + radius };
	}
};

template <typename simd_t>
struct wN_bounding_oriented_box
{
	wN_vec3<simd_t> center;
	wN_vec3<simd_t> radius;
	wN_quat<simd_t> rotation;
};

template <typename simd_t>
struct wN_line_segment
{
	wN_vec3<simd_t> a, b;
};

template <typename simd_t>
inline auto aabbVsAABB(const wN_bounding_box<simd_t>& a, const wN_bounding_box<simd_t>& b)
{
	return
		(a.maxCorner.x >= b.minCorner.x) & (a.minCorner.x <= b.maxCorner.x) &
		(a.maxCorner.y >= b.minCorner.y) & (a.minCorner.y <= b.maxCorner.y) &
		(a.maxCorner.z >= b.minCorner.z) & (a.minCorner.z <= b.maxCorner.z);
}

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

template <typename simd_t>
inline simd_t closestPoint_SegmentSegment(const wN_line_segment<simd_t>& l1, const wN_line_segment<simd_t>& l2, wN_vec3<simd_t>& c1, wN_vec3<simd_t>& c2)
{
	w_vec3 d1 = l1.b - l1.a;
	w_vec3 d2 = l2.b - l2.a;
	w_vec3 r = l1.a - l2.a;
	w_float a = dot(d1, d1);
	w_float e = dot(d2, d2);
	w_float f = dot(d2, r);

	w_float s, t;

	w_float c = dot(d1, r);

	w_float b = dot(d1, d2);
	w_float denom = a * e - b * b;

	s = ifThen(denom != 0.f, clamp01((b * f - c * e) / denom), 0.f);

	t = (b * s + f) / e;

	s = ifThen(t < 0.f, clamp01(-c / a), ifThen(t > 1.f, clamp01((b - c) / a), s));
	t = clamp01(t);


	auto eSmall = e <= EPSILON;
	auto aSmall = a <= EPSILON;

	s = ifThen(eSmall, clamp01(-c / a), s);
	t = ifThen(eSmall, 0.f, t);


	s = ifThen(aSmall, 0.f, s);
	t = ifThen(aSmall, clamp01(f / e), t);


	s = ifThen(aSmall & eSmall, 0.f, s);
	t = ifThen(aSmall & eSmall, 0.f, t);


	c1 = l1.a + d1 * s;
	c2 = l2.a + d2 * t;

	return squaredLength(c1 - c2);
}

