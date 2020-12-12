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

bool ray::intersectCylinder(const bounding_cylinder& cylinder, float& outT) const
{
	vec3 d = direction;
	vec3 o = origin;

	vec3 axis = cylinder.positionB - cylinder.positionA;
	float height = length(axis);

	quat q = rotateFromTo(axis, vec3(0.f, 1.f, 0.f));

	vec3 posA = cylinder.positionA;
	vec3 posB = posA + vec3(0.f, height, 0.f);

	o = q * (o - posA);
	d = q * d;

	float a = d.x * d.x + d.z * d.z;
	float b = d.x * o.x + d.z * o.z;
	float c = o.x * o.x + o.z * o.z - cylinder.radius * cylinder.radius;

	float delta = b * b - a * c;

	float epsilon = 1e-6f;

	if (delta < epsilon)
	{
		return false;
	}

	outT = (-b - sqrt(delta)) / a;
	if (outT <= epsilon)
	{
		return false; // Behind ray.
	}


	float y = o.y + outT * d.y;

	// Check bases.
	if (y > height + epsilon || y < -epsilon) 
	{
		ray localRay = { o, d };

		float dist;
		bool b1 = localRay.intersectDisk(posB, vec3(0.f, 1.f, 0.f), cylinder.radius, dist);
		if (b1)
		{
			outT = dist;
		}
		bool b2 = localRay.intersectDisk(posA, vec3(0.f, -1.f, 0.f), cylinder.radius, dist);
		if (b2 && dist > epsilon && outT >= dist)
		{
			outT = dist;
		}
	}

	y = o.y + outT * d.y;

	return y > -epsilon && y < height + epsilon;
}

bool ray::intersectDisk(vec3 pos, vec3 normal, float radius, float& outT) const
{
	bool intersectsPlane = intersectPlane(normal, pos, outT);
	if (intersectsPlane)
	{
		return length(origin + outT * direction - pos) <= radius;
	}
	return false;
}

bool ray::intersectRectangle(vec3 pos, vec3 tangent, vec3 bitangent, vec2 radius, float& outT) const
{
	vec3 normal = cross(tangent, bitangent);
	bool intersectsPlane = intersectPlane(normal, pos, outT);
	if (intersectsPlane)
	{
		vec3 offset = origin + outT * direction - pos;
		vec2 projected(dot(offset, tangent), dot(offset, bitangent));
		projected = abs(projected);
		if (projected.x <= radius.x && projected.y <= radius.y)
		{
			return true;
		}
	}
	return false;
}

static bool isZero(float x) 
{
	return abs(x) < 1e-6f;
}

struct solve_2_result
{
	uint32 numResults;
	float results[2];
};

struct solve_3_result
{
	uint32 numResults;
	float results[3];
};

struct solve_4_result
{
	uint32 numResults;
	float results[4];
};

static solve_2_result solve2(float c0, float c1, float c2)
{
	float p = c1 / (2 * c2);
	float q = c0 / c2;

	float D = p * p - q;

	if (isZero(D)) 
	{
		return { 1, -p };
	}
	else if (D < 0) 
	{
		return { 0 };
	}
	else /* if (D > 0) */ 
	{
		float sqrt_D = sqrt(D);

		return { 2, sqrt_D - p, -sqrt_D - p };
	}
}

static solve_3_result solve3(float c0, float c1, float c2, float c3)
{
	float A = c2 / c3;
	float B = c1 / c3;
	float C = c0 / c3;

	float sq_A = A * A;
	float p = 1.f / 3 * (-1.f / 3 * sq_A + B);
	float q = 1.f / 2 * (2.f / 27 * A * sq_A - 1.f / 3 * A * B + C);

	/* use Cardano's formula */

	float cb_p = p * p * p;
	float D = q * q + cb_p;

	solve_3_result s = {};

	if (isZero(D))
	{
		if (isZero(q))
		{
			s = { 1, 0.f };
		}
		else
		{
			float u = cbrt(-q);
			s = { 2, 2.f * u, -u };
		}
	}
	else if (D < 0) /* Casus irreducibilis: three real solutions */ 
	{
		float phi = 1.f / 3.f * acos(-q / sqrt(-cb_p));
		float t = 2.f * sqrt(-p);

		s = { 3,
			t * cos(phi),
			-t * cos(phi + M_PI / 3),
			-t * cos(phi - M_PI / 3) };

	}
	else /* one real solution */ 
	{
		float sqrt_D = sqrt(D);
		float u = cbrt(sqrt_D - q);
		float v = -cbrt(sqrt_D + q);

		s = { 1, u + v };

	}

	/* resubstitute */

	float sub = 1.f / 3.f * A;

	for (uint32 i = 0; i < s.numResults; ++i)
	{
		s.results[i] -= sub;
	}

	return s;
}

/**
 *  Solves equation:
 *
 *      c[0] + c[1]*x + c[2]*x^2 + c[3]*x^3 + c[4]*x^4 = 0
 *
 */
static solve_4_result solve4(float c0, float c1, float c2, float c3, float c4)
{
	/* normal form: x^4 + Ax^3 + Bx^2 + Cx + D = 0 */

	float A = c3 / c4;
	float B = c2 / c4;
	float C = c1 / c4;
	float D = c0 / c4;

	/*  substitute x = y - A/4 to eliminate cubic term:
	x^4 + px^2 + qx + r = 0 */

	float sq_A = A * A;
	float p = -3.f / 8 * sq_A + B;
	float q = 1.f / 8 * sq_A * A - 1.f / 2 * A * B + C;
	float r = -3.f / 256 * sq_A * sq_A + 1.f / 16 * sq_A * B - 1.f / 4 * A * C + D;
	solve_4_result s = {};

	if (isZero(r)) 
	{
		/* no absolute term: y(y^3 + py + q) = 0 */

		auto s3 = solve3(q, p, 0, 1);
		for (uint32 i = 0; i < s3.numResults; ++i)
		{
			s.results[s.numResults++] = s3.results[i];
		}

		s.results[s.numResults++] = 0.f;
	}
	else 
	{
		/* solve the resolvent cubic ... */
		
		auto s3 = solve3(1.f / 2 * r * p - 1.f / 8 * q * q, -r, -0.5f * p, 1.f);
		for (uint32 i = 0; i < s3.numResults; ++i)
		{
			s.results[s.numResults++] = s3.results[i];
		}

		/* ... and take the one real solution ... */

		float z = s.results[0];

		/* ... to build two quadric equations */

		float u = z * z - r;
		float v = 2.f * z - p;

		if (isZero(u))
		{
			u = 0;
		}
		else if (u > 0)
		{
			u = sqrt(u);
		}
		else
		{
			return {};
		}

		if (isZero(v))
		{
			v = 0;
		}
		else if (v > 0)
		{
			v = sqrt(v);
		}
		else
		{
			return {};
		}

		auto s2 = solve2(z - u, q < 0 ? -v : v, 1);
		s = {};
		for (uint32 i = 0; i < s2.numResults; ++i)
		{
			s.results[s.numResults++] = s2.results[i];
		}

		s2 = solve2(z + u, q < 0 ? v : -v, 1);
		for (uint32 i = 0; i < s2.numResults; ++i)
		{
			s.results[s.numResults++] = s2.results[i];
		}
	}

	/* resubstitute */

	float sub = 1.f / 4 * A;

	for (uint32 i = 0; i < s.numResults; ++i)
	{
		s.results[i] -= sub;
	}

	return s;
}

bool ray::intersectTorus(const bounding_torus& torus, float& outT) const
{
	vec3 d = direction;
	vec3 o = origin;

	vec3 axis = torus.upAxis;

	quat q = rotateFromTo(axis, vec3(0.f, 1.f, 0.f));

	o = q * (o - torus.position);
	d = q * d;



	// define the coefficients of the quartic equation
	float sum_d_sqrd = dot(d, d);

	float e = dot(o, o) - torus.majorRadius * torus.majorRadius - torus.tubeRadius * torus.tubeRadius;
	float f = dot(o, d);
	float four_a_sqrd = 4.f * torus.majorRadius * torus.majorRadius;

	auto solution = solve4(
		e * e - four_a_sqrd * (torus.tubeRadius * torus.tubeRadius - o.y * o.y),
		4.f * f * e + 2.f * four_a_sqrd * o.y * d.y,
		2.f * sum_d_sqrd * e + 4.f * f * f + four_a_sqrd * d.y * d.y,
		4.f * sum_d_sqrd * f,
		sum_d_sqrd * sum_d_sqrd
	);

	// ray misses the torus
	if (solution.numResults == 0)
	{
		return false;
	}

	// find the smallest root greater than kEpsilon, if any
	// the roots array is not sorted
	float minT = FLT_MAX;
	for (uint32 i = 0; i < solution.numResults; ++i) 
	{
		float t = solution.results[i];
		if ((t > 1e-6f) && (t < minT)) {
			minT = t;
		}
	}
	outT = minT;
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
