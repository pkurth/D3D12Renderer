#include "pch.h"
#include "bounding_volumes.h"

static bool pointInAABB(vec3 point, bounding_box aabb)
{
	return point.x >= aabb.minCorner.x
		&& point.y >= aabb.minCorner.y
		&& point.z >= aabb.minCorner.z
		&& point.x <= aabb.maxCorner.x
		&& point.y <= aabb.maxCorner.y
		&& point.z <= aabb.maxCorner.z;
}

static float closestPointOnLineSegment(vec3 point, vec3 lineA, vec3 lineB)
{
	vec3 ab = lineB - lineA;
	float t = dot(point - lineA, ab) / squaredLength(ab);
	t = clamp(t, 0.f, 1.f);
	return t;
}

void bounding_box::grow(vec3 o)
{
	minCorner.x = min(minCorner.x, o.x);
	minCorner.y = min(minCorner.y, o.y);
	minCorner.z = min(minCorner.z, o.z);
	maxCorner.x = max(maxCorner.x, o.x);
	maxCorner.y = max(maxCorner.y, o.y);
	maxCorner.z = max(maxCorner.z, o.z);
}

void bounding_box::pad(vec3 p)
{
	minCorner -= p;
	maxCorner += p;
}

vec3 bounding_box::getCenter() const
{
	return (minCorner + maxCorner) * 0.5f;
}

vec3 bounding_box::getRadius() const
{
	return (maxCorner - minCorner) * 0.5f;
}

bounding_box bounding_box::transform(quat rotation, vec3 translation) const
{
	bounding_box result = bounding_box::negativeInfinity();
	result.grow(rotation * minCorner + translation);
	result.grow(rotation * vec3(maxCorner.x, minCorner.y, minCorner.z) + translation);
	result.grow(rotation * vec3(minCorner.x, maxCorner.y, minCorner.z) + translation);
	result.grow(rotation * vec3(maxCorner.x, maxCorner.y, minCorner.z) + translation);
	result.grow(rotation * vec3(minCorner.x, minCorner.y, maxCorner.z) + translation);
	result.grow(rotation * vec3(maxCorner.x, minCorner.y, maxCorner.z) + translation);
	result.grow(rotation * vec3(minCorner.x, maxCorner.y, maxCorner.z) + translation);
	result.grow(rotation * maxCorner + translation);
	return result;
}

bounding_box_corners bounding_box::getCorners() const
{
	bounding_box_corners result;
	result.i = minCorner;
	result.x = vec3(maxCorner.x, minCorner.y, minCorner.z);
	result.y = vec3(minCorner.x, maxCorner.y, minCorner.z);
	result.xy = vec3(maxCorner.x, maxCorner.y, minCorner.z);
	result.z = vec3(minCorner.x, minCorner.y, maxCorner.z);
	result.xz = vec3(maxCorner.x, minCorner.y, maxCorner.z);
	result.yz = vec3(minCorner.x, maxCorner.y, maxCorner.z);
	result.xyz = maxCorner;
	return result;
}

bounding_box_corners bounding_box::getCorners(quat rotation, vec3 translation) const
{
	bounding_box_corners result;
	result.i = rotation * minCorner + translation;
	result.x = rotation * vec3(maxCorner.x, minCorner.y, minCorner.z) + translation;
	result.y = rotation * vec3(minCorner.x, maxCorner.y, minCorner.z) + translation;
	result.xy = rotation * vec3(maxCorner.x, maxCorner.y, minCorner.z) + translation;
	result.z = rotation * vec3(minCorner.x, minCorner.y, maxCorner.z) + translation;
	result.xz = rotation * vec3(maxCorner.x, minCorner.y, maxCorner.z) + translation;
	result.yz = rotation * vec3(minCorner.x, maxCorner.y, maxCorner.z) + translation;
	result.xyz = rotation * maxCorner + translation;
	return result;
}

bounding_box bounding_box::negativeInfinity()
{
	return bounding_box{ vec3(FLT_MAX, FLT_MAX, FLT_MAX), vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX) };
}

bounding_box bounding_box::fromMinMax(vec3 minCorner, vec3 maxCorner)
{
	return bounding_box{ minCorner, maxCorner };
}

bounding_box bounding_box::fromCenterRadius(vec3 center, vec3 radius)
{
	return bounding_box{ center - radius, center + radius };
}

bool ray::intersectPlane(vec3 normal, float d, float& outT) const
{
	float ndotd = dot(direction, normal);
	if (abs(ndotd) < 1e-6f)
	{
		return false;
	}

	outT = -(dot(origin, normal) + d) / dot(direction, normal);
	return true;
}

bool ray::intersectPlane(vec3 normal, vec3 point, float& outT) const
{
	float d = -dot(normal, point);
	return intersectPlane(normal, d, outT);
}

bool ray::intersectAABB(const bounding_box& a, float& outT) const
{
	vec3 invDir = vec3(1.f / direction.x, 1.f / direction.y, 1.f / direction.z); // This can be Inf (when one direction component is 0) but still works.

	float tx1 = (a.minCorner.x - origin.x) * invDir.x;
	float tx2 = (a.maxCorner.x - origin.x) * invDir.x;

	outT = min(tx1, tx2);
	float tmax = max(tx1, tx2);

	float ty1 = (a.minCorner.y - origin.y) * invDir.y;
	float ty2 = (a.maxCorner.y - origin.y) * invDir.y;

	outT = max(outT, min(ty1, ty2));
	tmax = min(tmax, max(ty1, ty2));

	float tz1 = (a.minCorner.z - origin.z) * invDir.z;
	float tz2 = (a.maxCorner.z - origin.z) * invDir.z;

	outT = max(outT, min(tz1, tz2));
	tmax = min(tmax, max(tz1, tz2));

	bool result = tmax >= outT && outT > 0.f;

	return result;
}

bool ray::intersectTriangle(vec3 a, vec3 b, vec3 c, float& outT, bool& outFrontFacing) const
{
	vec3 normal = noz(cross(b - a, c - a));
	float d = -dot(normal, a);

	float nDotR = dot(direction, normal);
	if (fabsf(nDotR) <= 1e-6f)
	{
		return false;
	}

	outT = -(dot(origin, normal) + d) / nDotR;

	vec3 q = origin + outT * direction;
	outFrontFacing = nDotR < 0.f;
	return outT >= 0.f && pointInTriangle(q, a, b, c);
}

bool ray::intersectSphere(vec3 center, float radius, float& outT) const
{
	vec3 m = origin - center;
	float b = dot(m, direction);
	float c = dot(m, m) - radius * radius;

	if (c > 0.f && b > 0.f)
	{
		return false;
	}

	float discr = b * b - c;

	if (discr < 0.f)
	{
		return false;
	}

	outT = -b - sqrt(discr);

	if (outT < 0.f)
	{
		outT = 0.f;
	}

	return true;
}

float signedDistanceToPlane(const vec3& p, const vec4& plane)
{
	return dot(vec4(p, 1.f), plane);
}

bool aabbVSAABB(const bounding_box& a, const bounding_box& b)
{
	if (a.maxCorner.x < b.minCorner.x || a.minCorner.x > b.maxCorner.x) return false;
	if (a.maxCorner.z < b.minCorner.z || a.minCorner.z > b.maxCorner.z) return false;
	if (a.maxCorner.y < b.minCorner.y || a.minCorner.y > b.maxCorner.y) return false;
	return true;
}

bool sphereVSSphere(const bounding_sphere& a, const bounding_sphere& b)
{
	vec3 d = a.center - b.center;
	float dist2 = dot(d, d);
	float radiusSum = a.radius + b.radius;
	return dist2 <= radiusSum * radiusSum;
}

bool sphereVSPlane(const bounding_sphere& s, const vec4& p)
{
	return abs(signedDistanceToPlane(s.center, p)) <= s.radius;
}

vec3 closestPoint_PointSegment(const vec3& q, const line_segment& l)
{
	vec3 ab = l.b - l.a;
	float t = dot(q - l.a, ab) / squaredLength(ab);
	t = clamp(t, 0.f, 1.f);
	return l.a + t * ab;
}

vec3 closestPoint_PointAABB(const vec3& q, const bounding_box& aabb)
{
	vec3 result;
	for (int i = 0; i < 3; i++)
	{
		float v = q.data[i];
		if (v < aabb.minCorner.data[i]) v = aabb.minCorner.data[i];
		if (v > aabb.maxCorner.data[i]) v = aabb.maxCorner.data[i];
		result.data[i] = v;
	}
	return result;
}

float closestPoint_SegmentSegment(const line_segment& l1, const line_segment& l2, vec3& c1, vec3& c2)
{
	float s, t;
	vec3 d1 = l1.b - l1.a; // Direction vector of segment S1
	vec3 d2 = l2.b - l2.a; // Direction vector of segment S2
	vec3 r = l1.a - l2.a;
	float a = dot(d1, d1); // Squared length of segment S1, always nonnegative
	float e = dot(d2, d2); // Squared length of segment S2, always nonnegative
	float f = dot(d2, r);
	// Check if either or both segments degenerate into points
	if (a <= EPSILON && e <= EPSILON)
	{
		// Both segments degenerate into points
		s = t = 0.0f;
		c1 = l1.a;
		c2 = l2.a;
		return dot(c1 - c2, c1 - c2);
	}
	if (a <= EPSILON)
	{
		// First segment degenerates into a point
		s = 0.0f;
		t = f / e; // s = 0 => t = (b*s + f) / e = f / e
		t = clamp(t, 0.f, 1.f);
	}
	else {
		float c = dot(d1, r);
		if (e <= EPSILON)
		{
			// Second segment degenerates into a point
			t = 0.0f;
			s = clamp(-c / a, 0.f, 1.f); // t = 0 => s = (b*t - c) / a = -c / a
		}
		else
		{
			// The general nondegenerate case starts here
			float b = dot(d1, d2);
			float denom = a * e - b * b; // Always nonnegative
			// If segments not parallel, compute closest point on L1 to L2 and
			// clamp to segment S1. Else pick arbitrary s (here 0)
			if (denom != 0.f)
				s = clamp((b * f - c * e) / denom, 0.f, 1.f);
			else
				s = 0.0f;
			// Compute point on L2 closest to S1(s) using
			// t = dot((l1.a + D1*s) - l2.a,D2) / dot(D2,D2) = (b*s + f) / e
			t = (b * s + f) / e;
			// If t in [0,1] done. Else clamp t, recompute s for the new value
			// of t using s = dot((l2.a + D2*t) - l1.a,D1) / dot(D1,D1)= (t*b - c) / a
			// and clamp s to [0, 1]
			if (t < 0.f) {
				t = 0.f;
				s = clamp(-c / a, 0.f, 1.f);
			}
			else if (t > 1.0f) {
				t = 1.f;
				s = clamp((b - c) / a, 0.f, 1.f);
			}
		}
	}
	c1 = l1.a + d1 * s;
	c2 = l2.a + d2 * t;
	return squaredLength(c1 - c2);
}
