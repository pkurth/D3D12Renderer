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

struct bounding_sphere
{
	vec3 center;
	float radius;

	float volume()
	{
		float sqRadius = radius * radius;
		float sqRadiusPI = M_PI * sqRadius;
		float sphereVolume = 4.f / 3.f * sqRadiusPI * radius;
		return sphereVolume;
	}
};

struct bounding_capsule
{
	vec3 positionA;
	vec3 positionB;
	float radius;

	float volume()
	{
		float sqRadius = radius * radius;
		float sqRadiusPI = M_PI * sqRadius;
		float sphereVolume = 4.f / 3.f * sqRadiusPI * radius;
		float height = length(positionA - positionB);
		float cylinderVolume = sqRadiusPI * height;
		return sphereVolume + cylinderVolume;
	}
};

struct bounding_cylinder
{
	vec3 positionA;
	vec3 positionB;
	float radius;
};

struct bounding_torus
{
	vec3 position;
	vec3 upAxis;
	float majorRadius;
	float tubeRadius;
};

union bounding_box_corners
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

	bounding_box_corners() {}
};

struct bounding_box
{
	vec3 minCorner;
	vec3 maxCorner;

	void grow(vec3 o);
	void pad(vec3 p);
	vec3 getCenter() const;
	vec3 getRadius() const;

	bounding_box transform(quat rotation, vec3 translation) const;
	bounding_box_corners getCorners() const;
	bounding_box_corners getCorners(quat rotation, vec3 translation) const;

	static bounding_box negativeInfinity();
	static bounding_box fromMinMax(vec3 minCorner, vec3 maxCorner);
	static bounding_box fromCenterRadius(vec3 center, vec3 radius);

	float volume()
	{
		vec3 extents = getRadius() * 2.f;
		return extents.x * extents.y * extents.z;
	}
};

struct bounding_hull
{
	vec3* vertices;
	indexed_triangle16* triangles;
	uint32 numVertices;
	uint32 numTriangles;
};

struct ray
{
	vec3 origin;
	vec3 direction;

	bool intersectPlane(vec3 normal, float d, float& outT) const;
	bool intersectPlane(vec3 normal, vec3 point, float& outT) const;
	bool intersectAABB(const bounding_box& a, float& outT) const;
	bool intersectTriangle(vec3 a, vec3 b, vec3 c, float& outT, bool& outFrontFacing) const;
	bool intersectSphere(vec3 center, float radius, float& outT) const;
	bool intersectSphere(const bounding_sphere& sphere, float& outT) const { return intersectSphere(sphere.center, sphere.radius, outT); }
	bool intersectCylinder(const bounding_cylinder& cylinder, float& outT) const;
	bool intersectDisk(vec3 pos, vec3 normal, float radius, float& outT) const;
	bool intersectRectangle(vec3 pos, vec3 tangent, vec3 bitangent, vec2 radius, float& outT) const;
	bool intersectTorus(const bounding_torus& torus, float& outT) const;
};

inline vec4 createPlane(vec3 point, vec3 normal)
{
	float d = -dot(normal, point);
	return vec4(normal, d);
}

float signedDistanceToPlane(const vec3& p, const vec4& plane);

bool aabbVSAABB(const bounding_box& a, const bounding_box& b);
bool sphereVSSphere(const bounding_sphere& a, const bounding_sphere& b);
bool sphereVSPlane(const bounding_sphere& s, const vec4& p);
vec3 closestPoint_PointSegment(const vec3& q, const line_segment& l);
vec3 closestPoint_PointAABB(const vec3& q, const bounding_box& aabb);
float closestPoint_SegmentSegment(const line_segment& l1, const line_segment& l2, vec3& c1, vec3& c2);
