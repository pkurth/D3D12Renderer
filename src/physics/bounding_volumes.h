#pragma once

#include "core/math.h"

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
	uint32 a, b;
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

struct bounding_oriented_box;

struct bounding_box
{
	vec3 minCorner;
	vec3 maxCorner;

	void grow(vec3 o);
	void pad(vec3 p);
	vec3 getCenter() const;
	vec3 getRadius() const;
	bool contains(vec3 p) const;

	bounding_box transformToAABB(quat rotation, vec3 translation) const;
	bounding_oriented_box transformToOBB(quat rotation, vec3 translation) const;
	bounding_box_corners getCorners() const;
	bounding_box_corners getCorners(quat rotation, vec3 translation) const;

	static bounding_box negativeInfinity();
	static bounding_box fromMinMax(vec3 minCorner, vec3 maxCorner);
	static bounding_box fromCenterRadius(vec3 center, vec3 radius);

	float volume()
	{
		vec3 diameter = maxCorner - minCorner;
		return diameter.x * diameter.y * diameter.z;
	}
};

struct bounding_oriented_box
{
	vec3 center;
	vec3 radius;
	quat rotation;

	float volume()
	{
		vec3 diameter = radius * 2.f;
		return diameter.x * diameter.y * diameter.z;
	}

	bounding_box getAABB() const;
	bounding_box transformToAABB(quat rotation, vec3 translation) const;
	bounding_oriented_box transformToOBB(quat rotation, vec3 translation) const;
	bounding_box_corners getCorners() const;
};

struct bounding_rectangle
{
	vec2 minCorner;
	vec2 maxCorner;

	void grow(vec2 o);
	void pad(vec2 p);
	vec2 getCenter() const;
	vec2 getRadius() const;
	bool contains(vec2 p) const;

	static bounding_rectangle negativeInfinity();
	static bounding_rectangle fromMinMax(vec2 minCorner, vec2 maxCorner);
	static bounding_rectangle fromCenterRadius(vec2 center, vec2 radius);
};

inline vec4 createPlane(vec3 point, vec3 normal)
{
	float d = -dot(normal, point);
	return vec4(normal, d);
}

inline vec3 createLine(vec2 point, vec2 normal)
{
	float d = -dot(normal, point);
	return vec3(normal, d);
}

float signedDistanceToPlane(const vec3& p, const vec4& plane);

struct bounding_plane
{
	vec4 plane;

	bounding_plane() {}
	bounding_plane(vec3 point, vec3 normal)
	{
		plane = createPlane(point, normal);
	}

	float signedDistance(vec3 p)
	{
		return signedDistanceToPlane(p, plane);
	}
};

struct bounding_hull_edge
{
	uint16 from, to;
	uint16 faceA, faceB;
};

struct bounding_hull_face
{
	uint16 a, b, c;
	vec3 normal;
};

struct bounding_hull_geometry
{
	std::vector<vec3> vertices;
	std::vector<bounding_hull_edge> edges;
	std::vector<bounding_hull_face> faces;

	bounding_box aabb;
};

// MUST be convex.
struct bounding_hull
{
	quat rotation;
	vec3 position;

	union
	{
		uint32 geometryIndex;						// For permanent colliders.
		const bounding_hull_geometry* geometryPtr;	// For world space colliders, which only exist for one frame.
	};
};

bounding_hull_geometry boundingHullFromMesh(vec3* vertices, uint32 numVertices, indexed_triangle32* triangles, uint32 numTriangles);

struct ray
{
	vec3 origin;
	vec3 direction;

	bool intersectPlane(vec3 normal, float d, float& outT) const;
	bool intersectPlane(vec3 normal, vec3 point, float& outT) const;
	bool intersectAABB(const bounding_box& a, float& outT) const;
	bool intersectOBB(const bounding_oriented_box& a, float& outT) const;
	bool intersectTriangle(vec3 a, vec3 b, vec3 c, float& outT, bool& outFrontFacing) const;
	bool intersectSphere(vec3 center, float radius, float& outT) const;
	bool intersectSphere(const bounding_sphere& sphere, float& outT) const { return intersectSphere(sphere.center, sphere.radius, outT); }
	bool intersectCylinder(const bounding_cylinder& cylinder, float& outT) const;
	bool intersectCapsule(const bounding_capsule& capsule, float& outT) const;
	bool intersectDisk(vec3 pos, vec3 normal, float radius, float& outT) const;
	bool intersectRectangle(vec3 pos, vec3 tangent, vec3 bitangent, vec2 radius, float& outT) const;
	bool intersectTorus(const bounding_torus& torus, float& outT) const;
	bool intersectHull(const bounding_hull& hull, const bounding_hull_geometry& geometry, float& outT) const;
};

bool aabbVSAABB(const bounding_box& a, const bounding_box& b);
bool sphereVSSphere(const bounding_sphere& a, const bounding_sphere& b);
bool sphereVSPlane(const bounding_sphere& s, const vec4& p);
vec3 closestPoint_PointSegment(const vec3& q, const line_segment& l);
vec3 closestPoint_PointAABB(const vec3& q, const bounding_box& aabb);
float closestPoint_SegmentSegment(const line_segment& l1, const line_segment& l2, vec3& c1, vec3& c2);
