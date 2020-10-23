#pragma once

#include "math.h"

struct indexed_triangle16
{
	uint16 a, b, c;
};

struct indexed_triangle32
{
	uint32 a, b, c;
};

struct indexed_line16
{
	uint16 a, b;
};

struct indexed_line32
{
	uint32 a, b, c;
};

struct line_segment
{
	vec3 a, b;
};

struct sphere_collider
{
	vec3 center;
	float radius;
};

struct capsule_collider
{
	vec3 positionA;
	vec3 positionB;
	float radius;
};

union aabb_corners
{
	struct
	{
		vec3 i;
		vec3 x;
		vec3 y;
		vec3 xy;
		vec3 z;
		vec3 xz;
		vec3 yz;
		vec3 xyz;
	};
	vec3 corners[8];

	aabb_corners() {}
};

struct aabb_collider
{
	vec3 minCorner;
	vec3 maxCorner;

	void grow(vec3 o);
	void pad(vec3 p);
	vec3 getCenter() const;
	vec3 getRadius() const;

	aabb_collider transform(quat rotation, vec3 translation) const;
	aabb_corners getCorners() const;
	aabb_corners getCorners(quat rotation, vec3 translation) const;

	static aabb_collider negativeInfinity();
	static aabb_collider fromMinMax(vec3 minCorner, vec3 maxCorner);
	static aabb_collider fromCenterRadius(vec3 center, vec3 radius);
};

struct convex_hull_collider
{
	vec3* vertices;
	indexed_triangle16* triangles;
	uint32 numVertices;
	uint32 numTriangles;
};

struct plane_collider
{
	vec3 normal;
	float d;

	plane_collider() {}

	plane_collider(const vec3& point, const vec3& normal);
	float signedDistance(const vec3& p) const;
	bool isFrontFacingTo(const vec3& dir) const;
	vec3 getPointOnPlane() const;
};

enum collider_type : uint16
{
	collider_type_sphere,
	collider_type_capsule,
	collider_type_aabb,
};

struct collider_base
{
	union
	{
		sphere_collider sphere;
		capsule_collider capsule;
		aabb_collider aabb;
	};

	collider_type type;
	uint16 rigidBodyID;

	collider_base() {}
};

struct collider_properties
{
	float restitution;
	float friction;
	float density;
};

struct ray
{
	vec3 origin;
	vec3 direction;

	bool intersectPlane(vec3 normal, float d, float& outT) const;
	bool intersectPlane(vec3 normal, vec3 point, float& outT) const;
	bool intersectAABB(const aabb_collider& a, float& outT) const;
	bool intersectTriangle(vec3 a, vec3 b, vec3 c, float& outT, bool& outFrontFacing) const;
	bool intersectSphere(vec3 center, float radius, float& outT) const;
	bool intersectSphere(const sphere_collider& sphere, float& outT) const { return intersectSphere(sphere.center, sphere.radius, outT); }
};
