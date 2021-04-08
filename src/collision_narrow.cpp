#include "pch.h"
#include "collision_narrow.h"
#include "physics.h"
#include "collision_broad.h"
#include "collision_gjk.h"
#include "collision_epa.h"
#include "collision_sat.h"

#include "render_pass.h"
#include "pbr.h"

struct debug_draw_call
{
	submesh_info submesh;
	ref<pbr_material> material;
	mat4 transform;
};

static std::vector<debug_draw_call> debugDrawCalls;

static dx_mesh debugMesh;

static submesh_info sphereMesh;

static ref<pbr_material> redMaterial;
static ref<pbr_material> greenMaterial;
static ref<pbr_material> blueMaterial;

static void debugSphere(vec3 position, float radius, ref<pbr_material> material)
{
	debugDrawCalls.push_back({
		sphereMesh,
		material,
		createModelMatrix(position, quat::identity, radius),
	});
}



static bool intersection(const bounding_oriented_box& a, const bounding_oriented_box& b, contact_manifold& outContact);

static void getTangents(vec3 normal, vec3& outTangent, vec3& outBitangent)
{
	if (abs(normal.x) >= 0.57735f)
	{
		outTangent = vec3(normal.y, -normal.x, 0.f);
	}
	else
	{
		outTangent = vec3(0.f, normal.z, -normal.y);
	}

	outTangent = normalize(outTangent);
	outBitangent = cross(normal, outTangent);
}

struct vertex_penetration_pair
{
	vec3 vertex;
	float penetrationDepth;
};

static void findStableContactManifold(vertex_penetration_pair* vertices, uint32 numVertices, vec3 normal, vec3 tangent, contact_manifold& outContact)
{
	// http://media.steampowered.com/apps/valve/2015/DirkGregorius_Contacts.pdf slide 103ff.

	if (numVertices > 4)
	{
		// Find first vertex along some fixed distance.
		vec3 searchDir = tangent;
		float bestDistance = dot(searchDir, vertices[0].vertex);
		uint32 resultIndex = 0;
		for (uint32 i = 1; i < numVertices; ++i)
		{
			float distance = dot(searchDir, vertices[i].vertex);
			if (distance > bestDistance)
			{
				resultIndex = i;
				bestDistance = distance;
			}
		}

		outContact.contacts[0].penetrationDepth = vertices[resultIndex].penetrationDepth;
		outContact.contacts[0].point = vertices[resultIndex].vertex;

		// Find second point which is furthest away from first.
		bestDistance = 0.f;
		resultIndex = 0;
		for (uint32 i = 0; i < numVertices; ++i)
		{
			float sqDistance = squaredLength(vertices[i].vertex - outContact.contacts[0].point);
			if (sqDistance > bestDistance)
			{
				resultIndex = i;
				bestDistance = sqDistance;
			}
		}

		outContact.contacts[1].penetrationDepth = vertices[resultIndex].penetrationDepth;
		outContact.contacts[1].point = vertices[resultIndex].vertex;

		// Find third point which maximizes the area of the resulting triangle.
		float bestArea = 0.f;
		resultIndex = 0;
		for (uint32 i = 0; i < numVertices; ++i)
		{
			vec3 qa = outContact.contacts[0].point - vertices[i].vertex;
			vec3 qb = outContact.contacts[1].point - vertices[i].vertex;
			float area = 0.5f * dot(cross(qa, qb), normal);
			if (area > bestArea)
			{
				resultIndex = i;
				bestArea = area;
			}
		}

		outContact.contacts[2].penetrationDepth = vertices[resultIndex].penetrationDepth;
		outContact.contacts[2].point = vertices[resultIndex].vertex;

		// Find fourth point.
		bestArea = 0.f;
		resultIndex = 0;
		for (uint32 i = 0; i < numVertices; ++i)
		{
			vec3 qa = outContact.contacts[0].point - vertices[i].vertex;
			vec3 qb = outContact.contacts[1].point - vertices[i].vertex;
			vec3 qc = outContact.contacts[2].point - vertices[i].vertex;
			float area1 = 0.5f * dot(cross(qa, qb), normal);
			float area2 = 0.5f * dot(cross(qb, qc), normal);
			float area3 = 0.5f * dot(cross(qc, qa), normal);
			float area = max(max(area1, area2), area3);
			if (area > bestArea)
			{
				resultIndex = i;
				bestArea = area;
			}
		}

		outContact.contacts[3].penetrationDepth = vertices[resultIndex].penetrationDepth;
		outContact.contacts[3].point = vertices[resultIndex].vertex;

		outContact.numContacts = 4;
	}
	else
	{
		outContact.numContacts = numVertices;
		for (uint32 i = 0; i < numVertices; ++i)
		{
			outContact.contacts[i].penetrationDepth = vertices[i].penetrationDepth;
			outContact.contacts[i].point = vertices[i].vertex;
		}
	}
}

struct clipping_polygon
{
	vertex_penetration_pair points[64];
	uint32 numPoints = 0;
};

static vertex_penetration_pair clipAgainstPlane(vertex_penetration_pair a, vertex_penetration_pair b, float aDist, float bDist)
{
	aDist = abs(aDist);
	bDist = abs(bDist);
	
	float total = aDist + bDist;
	float t = aDist / total;

	return { lerp(a.vertex, b.vertex, t), lerp(a.penetrationDepth, b.penetrationDepth, t) };
}

// Planes must point inside.
static void sutherlandHodgmanClipping(clipping_polygon& input, vec4* clipPlanes, uint32 numClipPlanes, clipping_polygon& output)
{
	clipping_polygon* in = &input;
	clipping_polygon* out = &output;

	uint32 clipIndex = 0;
	for (; clipIndex < numClipPlanes; ++clipIndex)
	{
		vec4 clipPlane = clipPlanes[clipIndex];
		out->numPoints = 0;

		if (in->numPoints == 0)
		{
			break;
		}

		vertex_penetration_pair startPoint = in->points[in->numPoints - 1];
		for (uint32 i = 0; i < in->numPoints; ++i)
		{
			vertex_penetration_pair endPoint = in->points[i];

			float startDist = signedDistanceToPlane(startPoint.vertex, clipPlane);
			float endDist = signedDistanceToPlane(endPoint.vertex, clipPlane);
			bool startInside = startDist > 0.f;
			bool endInside = endDist > 0.f;

			if (startInside && endInside)
			{
				out->points[out->numPoints++] = endPoint;
			}
			else if (startInside)
			{
				out->points[out->numPoints++] = clipAgainstPlane(startPoint, endPoint, startDist, endDist);
			}
			else if (!startInside && endInside)
			{
				out->points[out->numPoints++] = clipAgainstPlane(startPoint, endPoint, startDist, endDist);
				out->points[out->numPoints++] = endPoint;
			}

			startPoint = endPoint;
		}

		clipping_polygon* tmp = in;
		in = out;
		out = tmp;
	}

	if (clipIndex % 2 == 0)
	{
		for (uint32 i = 0; i < input.numPoints; ++i)
		{
			output.points[i] = input.points[i];
		}
		output.numPoints = input.numPoints;
	}
}

// Returns points and normals in AABB's local space. The caller must transform them to world space if needed.
static void getAABBClippingPlanes(vec3 aabbRadius, vec3 normal, vec3* clipPlanePoints, vec3* clipPlaneNormals)
{
	vec3 p = abs(normal);

	uint32 maxElement = (p.x > p.y) ? ((p.x > p.z) ? 0 : 2) : ((p.y > p.z) ? 1 : 2);

	uint32 axis0 = (maxElement + 1) % 3;
	uint32 axis1 = (maxElement + 2) % 3;

	{
		vec3 planeNormal(0.f); planeNormal.data[axis0] = 1.f;
		clipPlaneNormals[0] = planeNormal;
		clipPlanePoints[0] = -aabbRadius;
	}
	{
		vec3 planeNormal(0.f); planeNormal.data[axis1] = 1.f;
		clipPlaneNormals[1] = planeNormal;
		clipPlanePoints[1] = -aabbRadius;
	}
	{
		vec3 planeNormal(0.f); planeNormal.data[axis0] = -1.f;
		clipPlaneNormals[2] = planeNormal;
		clipPlanePoints[2] = aabbRadius;
	}
	{
		vec3 planeNormal(0.f); planeNormal.data[axis1] = -1.f;
		clipPlaneNormals[3] = planeNormal;
		clipPlanePoints[3] = aabbRadius;
	}
}

// Returns points in AABB's local space. The caller must transform them to world space if needed.
static void getAABBIncidentVertices(vec3 aabbRadius, vec3 normal, clipping_polygon& polygon)
{
	vec3 p = abs(normal);

	uint32 maxElement = (p.x > p.y) ? ((p.x > p.z) ? 0 : 2) : ((p.y > p.z) ? 1 : 2);
	float s = normal.data[maxElement] < 0.f ? 1.f : -1.f; // Flipped sign.

	uint32 axis0 = (maxElement + 1) % 3;
	uint32 axis1 = (maxElement + 2) % 3;

	float d = aabbRadius.data[maxElement] * s;
	float min0 = -aabbRadius.data[axis0];
	float min1 = -aabbRadius.data[axis1];
	float max0 = aabbRadius.data[axis0];
	float max1 = aabbRadius.data[axis1];

	polygon.numPoints = 4;
	polygon.points[0].vertex.data[maxElement] = d;
	polygon.points[0].vertex.data[axis0] = min0;
	polygon.points[0].vertex.data[axis1] = min1;

	polygon.points[1].vertex.data[maxElement] = d;
	polygon.points[1].vertex.data[axis0] = max0;
	polygon.points[1].vertex.data[axis1] = min1;

	polygon.points[2].vertex.data[maxElement] = d;
	polygon.points[2].vertex.data[axis0] = max0;
	polygon.points[2].vertex.data[axis1] = max1;

	polygon.points[3].vertex.data[maxElement] = d;
	polygon.points[3].vertex.data[axis0] = min0;
	polygon.points[3].vertex.data[axis1] = max1;
}

static void getAABBIncidentEdge(vec3 aabbRadius, vec3 normal, vec3& outA, vec3& outB)
{
	vec3 p = abs(normal);

	outA = vec3(aabbRadius.x, aabbRadius.y, aabbRadius.z);

	if (p.x > p.y)
	{
		if (p.y > p.z)
		{
			outB = vec3(aabbRadius.x, aabbRadius.y, -aabbRadius.z);
		}
		else
		{
			outB = vec3(aabbRadius.x, -aabbRadius.y, aabbRadius.z);
		}
	}
	else
	{
		if (p.x > p.z)
		{
			outB = vec3(aabbRadius.x, aabbRadius.y, -aabbRadius.z);
		}
		else
		{
			outB = vec3(-aabbRadius.x, aabbRadius.y, aabbRadius.z);
		}
	}

	float sx = normal.x < 0.f ? -1.f : 1.f;
	float sy = normal.y < 0.f ? -1.f : 1.f;
	float sz = normal.z < 0.f ? -1.f : 1.f;

	outA *= vec3(sx, sy, sz);
	outB *= vec3(sx, sy, sz);
}


// Sphere tests.
static bool intersection(const bounding_sphere& s1, const bounding_sphere& s2, contact_manifold& outContact)
{
	vec3 n = s2.center - s1.center;
	float radiusSum = s2.radius + s1.radius;
	float sqDistance = squaredLength(n);
	if (sqDistance <= radiusSum * radiusSum)
	{
		float distance;
		if (sqDistance == 0.f) // Degenerate case.
		{
			distance = 0.f;
			outContact.collisionNormal = vec3(0.f, 1.f, 0.f); // Up.
		}
		else
		{
			distance = sqrt(sqDistance);
			outContact.collisionNormal = n / distance;
		}

		getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);
		outContact.numContacts = 1;
		outContact.contacts[0].penetrationDepth = radiusSum - distance; // Flipped to change sign.
		assert(outContact.contacts[0].penetrationDepth >= 0.f);
		outContact.contacts[0].point = 0.5f * (s1.center + s1.radius * outContact.collisionNormal + s2.center - s2.radius * outContact.collisionNormal);
		return true;
	}
	return false;
}

static bool intersection(const bounding_sphere& s, const bounding_capsule& c, contact_manifold& outContact)
{
	vec3 closestPoint = closestPoint_PointSegment(s.center, line_segment{ c.positionA, c.positionB });
	return intersection(s, bounding_sphere{ closestPoint, c.radius }, outContact);
}

static bool intersection(const bounding_sphere& s, const bounding_box& a, contact_manifold& outContact)
{
	vec3 p = closestPoint_PointAABB(s.center, a);
	vec3 n = p - s.center;
	float sqDistance = squaredLength(n);
	if (sqDistance <= s.radius * s.radius)
	{
		float dist = 0.f;
		if (sqDistance > 0.f)
		{
			dist = sqrt(sqDistance);
			n /= dist;
		}
		else
		{
			n = vec3(0.f, 1.f, 0.f);
		}

		outContact.numContacts = 1;

		outContact.collisionNormal = n;
		outContact.contacts[0].penetrationDepth = s.radius - dist; // Flipped to change sign.
		outContact.contacts[0].point = 0.5f * (p + s.center + n * s.radius);
		getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);

		return true;
	}
	return false;
}

static bool intersection(const bounding_sphere& s, const bounding_oriented_box& o, contact_manifold& outContact)
{
	bounding_box aabb = bounding_box::fromCenterRadius(o.center, o.radius);
	bounding_sphere s_ = { 
		conjugate(o.rotation)* (s.center - o.center) + o.center,
		s.radius };
	
	if (intersection(s_, aabb, outContact))
	{
		outContact.collisionNormal = o.rotation * outContact.collisionNormal;
		outContact.collisionTangent = o.rotation * outContact.collisionTangent;
		outContact.collisionBitangent = o.rotation * outContact.collisionBitangent;
		outContact.contacts[0].point = o.rotation * (outContact.contacts[0].point - o.center) + o.center;
		return true;
	}
	return false;
}

// Capsule tests.
static bool intersection(const bounding_capsule& c1, const bounding_capsule& c2, contact_manifold& outContact)
{
	vec3 closestPoint1, closestPoint2;
	closestPoint_SegmentSegment(line_segment{ c1.positionA, c1.positionB }, line_segment{ c2.positionA, c2.positionB }, closestPoint1, closestPoint2);
	return intersection(bounding_sphere{ closestPoint1, c1.radius }, bounding_sphere{ closestPoint2, c2.radius }, outContact);
}

static bool intersection(const bounding_capsule& c, const bounding_box& a, contact_manifold& outContact)
{
	capsule_support_fn capsuleSupport{ c };
	aabb_support_fn boxSupport{ a };

	gjk_simplex gjkSimplex;
	if (!gjkIntersectionTest(capsuleSupport, boxSupport, gjkSimplex))
	{
		return false;
	}

	epa_result epa;
	auto epaSuccess = epaCollisionInfo(gjkSimplex, capsuleSupport, boxSupport, epa);
	if (epaSuccess != epa_success)
	{
		//return false;
	}

	outContact.collisionNormal = epa.normal;
	getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);
	outContact.numContacts = 1;
	outContact.contacts[0].penetrationDepth = epa.penetrationDepth;
	outContact.contacts[0].point = epa.point;

	return true;
}

static bool intersection(const bounding_capsule& c, const bounding_oriented_box& o, contact_manifold& outContact)
{
	bounding_box aabb = bounding_box::fromCenterRadius(o.center, o.radius);
	bounding_capsule c_ = { 
		conjugate(o.rotation) * (c.positionA - o.center) + o.center, 
		conjugate(o.rotation) * (c.positionB - o.center) + o.center,
		c.radius };

	if (intersection(c_, aabb, outContact))
	{
		outContact.collisionNormal = o.rotation * outContact.collisionNormal;
		outContact.collisionTangent = o.rotation * outContact.collisionTangent;
		outContact.collisionBitangent = o.rotation * outContact.collisionBitangent;

		for (uint32 i = 0; i < outContact.numContacts; ++i)
		{
			outContact.contacts[i].point = o.rotation * (outContact.contacts[i].point - o.center) + o.center;
		}
		return true;
	}
	return false;
}

// AABB tests.
static bool intersection(const bounding_box& a, const bounding_box& b, contact_manifold& outContact)
{
	vec3 centerA = a.getCenter();
	vec3 centerB = b.getCenter();

	vec3 radiusA = a.getRadius();
	vec3 radiusB = b.getRadius();

	vec3 d = centerB - centerA;
	vec3 p = (radiusB + radiusA) - abs(d);

	if (p.x < 0.f || p.y < 0.f || p.z < 0.f)
	{
		return false;
	}

	uint32 minElement = (p.x < p.y) ? ((p.x < p.z) ? 0 : 2) : ((p.y < p.z) ? 1 : 2);

	float s = d.data[minElement] < 0.f ? -1.f : 1.f;
	float penetration = p.data[minElement] * s;
	vec3 normal(0.f);
	normal.data[minElement] = s;

	outContact.collisionNormal = normal;
	getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);
	outContact.numContacts = 4;

	uint32 axis0 = (minElement + 1) % 3;
	uint32 axis1 = (minElement + 2) % 3;

	float min0 = max(a.minCorner.data[axis0], b.minCorner.data[axis0]);
	float min1 = max(a.minCorner.data[axis1], b.minCorner.data[axis1]);
	float max0 = min(a.maxCorner.data[axis0], b.maxCorner.data[axis0]);
	float max1 = min(a.maxCorner.data[axis1], b.maxCorner.data[axis1]);

	float depth = centerA.data[minElement] + radiusA.data[minElement] - penetration * 0.5f;

	outContact.contacts[0].penetrationDepth = penetration;
	outContact.contacts[0].point = vec3(0.f);
	outContact.contacts[0].point.data[axis0] = min0;
	outContact.contacts[0].point.data[axis1] = min1;
	outContact.contacts[0].point.data[minElement] = depth;

	outContact.contacts[1].penetrationDepth = penetration;
	outContact.contacts[1].point = vec3(0.f);
	outContact.contacts[1].point.data[axis0] = min0;
	outContact.contacts[1].point.data[axis1] = max1;
	outContact.contacts[1].point.data[minElement] = depth;

	outContact.contacts[2].penetrationDepth = penetration;
	outContact.contacts[2].point = vec3(0.f);
	outContact.contacts[2].point.data[axis0] = max0;
	outContact.contacts[2].point.data[axis1] = min1;
	outContact.contacts[2].point.data[minElement] = depth;

	outContact.contacts[3].penetrationDepth = penetration;
	outContact.contacts[3].point = vec3(0.f);
	outContact.contacts[3].point.data[axis0] = max0;
	outContact.contacts[3].point.data[axis1] = max1;
	outContact.contacts[3].point.data[minElement] = depth;

	debugSphere(outContact.contacts[0].point, 0.1f, blueMaterial);
	debugSphere(outContact.contacts[1].point, 0.1f, blueMaterial);
	debugSphere(outContact.contacts[2].point, 0.1f, blueMaterial);
	debugSphere(outContact.contacts[3].point, 0.1f, blueMaterial);

	return true;
}

static bool intersection(const bounding_box& a, const bounding_oriented_box& b, contact_manifold& outContact)
{
	// We forward to the more general case OBB vs OBB here. This is not ideal, since this test then again transforms to a space local
	// to one OOB.
	// However, I don't expect this function to be called very often, as AABBs are uncommon, so this is probably fine.
	return intersection(bounding_oriented_box{ a.getCenter(), a.getRadius(), quat::identity }, b, outContact);
}

// OBB tests.
static bool intersection(const bounding_oriented_box& a, const bounding_oriented_box& b, contact_manifold& outContact)
{
	union obb_axes
	{
		struct
		{
			vec3 x, y, z;
		};
		vec3 u[3];
	};

	obb_axes axesA = {
		a.rotation * vec3(1.f, 0.f, 0.f),
		a.rotation * vec3(0.f, 1.f, 0.f),
		a.rotation * vec3(0.f, 0.f, 1.f),
	};

	obb_axes axesB = {
		b.rotation * vec3(1.f, 0.f, 0.f),
		b.rotation * vec3(0.f, 1.f, 0.f),
		b.rotation * vec3(0.f, 0.f, 1.f),
	};

	mat3 r;
	r.m00 = dot(axesA.x, axesB.x);
	r.m10 = dot(axesA.y, axesB.x);
	r.m20 = dot(axesA.z, axesB.x);
	r.m01 = dot(axesA.x, axesB.y);
	r.m11 = dot(axesA.y, axesB.y);
	r.m21 = dot(axesA.z, axesB.y);
	r.m02 = dot(axesA.x, axesB.z);
	r.m12 = dot(axesA.y, axesB.z);
	r.m22 = dot(axesA.z, axesB.z);

	vec3 tw = b.center - a.center;
	vec3 t = conjugate(a.rotation) * tw;

	bool parallel = false;

	mat3 absR;
	for (uint32 i = 0; i < 9; ++i)
	{
		absR.m[i] = abs(r.m[i]) + EPSILON; // Add in an epsilon term to counteract arithmetic errors when two edges are parallel and their cross product is (near) 0.
		if (absR.m[i] >= 0.99f)
		{
			parallel = true;
		}
	}

	float ra, rb;

	float minPenetration = FLT_MAX;
	vec3 normal;
	bool bFace = false;


	// Test a's faces.
	for (uint32 i = 0; i < 3; ++i)
	{
		ra = a.radius.data[i];
		rb = dot(row(absR, i), b.radius);
		float d = t.data[i];
		float penetration = ra + rb - abs(d);
		if (penetration < 0.f) { return false; }
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			normal = vec3(0.f); normal.data[i] = 1.f;
		}
	}

	// Test b's faces.
	for (uint32 i = 0; i < 3; ++i)
	{
		ra = dot(col(absR, i), a.radius);
		rb = b.radius.data[i];
		float d = dot(col(r, i), t);
		float penetration = ra + rb - abs(d);
		if (penetration < 0.f) { return false; }
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			normal = vec3(0.f); normal.data[i] = 1.f;
			bFace = true;
		}
	}

	bool edgeCollision = false;
	vec3 edgeNormal;

	if (!parallel)
	{
		float penetration;
		vec3 normal;
		float l;

		// Test a.x x b.x.
		ra = a.radius.y * absR.m20 + a.radius.z * absR.m10;
		rb = b.radius.y * absR.m02 + b.radius.z * absR.m01;
		penetration = ra + rb - abs(t.z * r.m10 - t.y * r.m20);
		if (penetration < 0.f) { return false; }
		normal = vec3(0.f, -r.m20, r.m10);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}

		// Test a.x x b.y.
		ra = a.radius.y * absR.m21 + a.radius.z * absR.m11;
		rb = b.radius.x * absR.m02 + b.radius.z * absR.m00;
		penetration = ra + rb - abs(t.z * r.m11 - t.y * r.m21);
		if (penetration < 0.f) { return false; }
		normal = vec3(0.f, -r.m21, r.m11);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}

		// Test a.x x b.z.
		ra = a.radius.y * absR.m22 + a.radius.z * absR.m12;
		rb = b.radius.x * absR.m01 + b.radius.y * absR.m00;
		penetration = ra + rb - abs(t.z * r.m12 - t.y * r.m22);
		if (penetration < 0.f) { return false; }
		normal = vec3(0.f, -r.m22, r.m12);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}

		// Test a.y x b.x.
		ra = a.radius.x * absR.m20 + a.radius.z * absR.m00;
		rb = b.radius.y * absR.m12 + b.radius.z * absR.m11;
		penetration = ra + rb - abs(t.x * r.m20 - t.z * r.m00);
		if (penetration < 0.f) { return false; }
		normal = vec3(r.m20, 0.f, -r.m00);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}

		// Test a.y x b.y.
		ra = a.radius.x * absR.m21 + a.radius.z * absR.m01;
		rb = b.radius.x * absR.m12 + b.radius.z * absR.m10;
		penetration = ra + rb - abs(t.x * r.m21 - t.z * r.m01);
		if (penetration < 0.f) { return false; }
		normal = vec3(r.m21, 0.f, -r.m01);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}

		// Test a.y x b.z.
		ra = a.radius.x * absR.m22 + a.radius.z * absR.m02;
		rb = b.radius.x * absR.m11 + b.radius.y * absR.m10;
		penetration = ra + rb - abs(t.x * r.m22 - t.z * r.m02);
		if (penetration < 0.f) { return false; }
		normal = vec3(r.m22, 0.f, -r.m02);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}

		// Test a.z x b.x.
		ra = a.radius.x * absR.m10 + a.radius.y * absR.m00;
		rb = b.radius.y * absR.m22 + b.radius.z * absR.m21;
		penetration = ra + rb - abs(t.y * r.m00 - t.x * r.m10);
		if (penetration < 0.f) { return false; }
		normal = vec3(-r.m10, r.m00, 0.f);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}

		// Test a.z x b.y.
		ra = a.radius.x * absR.m11 + a.radius.y * absR.m01;
		rb = b.radius.x * absR.m22 + b.radius.z * absR.m20;
		penetration = ra + rb - abs(t.y * r.m01 - t.x * r.m11);
		if (penetration < 0.f) { return false; }
		normal = vec3(-r.m11, r.m01, 0.f);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}

		// Test a.z x b.z.
		ra = a.radius.x * absR.m12 + a.radius.y * absR.m02;
		rb = b.radius.x * absR.m21 + b.radius.y * absR.m20;
		penetration = ra + rb - abs(t.y * r.m02 - t.x * r.m12);
		if (penetration < 0.f) { return false; }
		normal = vec3(-r.m12, r.m02, 0.f);
		l = 1.f / length(normal);
		penetration *= l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			edgeNormal = normal * l;
			edgeCollision = true;
		}
	}


	bool faceCollision = !edgeCollision;
	if (faceCollision)
	{
		if (bFace)
		{
			normal = r * normal;
		}
	}
	else
	{
		normal = edgeNormal;
	}

	normal = a.rotation * normal;

	//normal = normalize(normal);

	if (dot(normal, tw) < 0.f)
	{
		normal = -normal;
	}

	// Normal is now in world space and points from a to b.

	outContact.collisionNormal = normal;
	getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);

	if (faceCollision)
	{
		//std::cout << "FACE SHIT " << normal << "\n";

		vec3 clipPlanePoints[4];
		vec3 clipPlaneNormals[4];

		clipping_polygon polygon;

		vec4 plane;

		if (!bFace)
		{
			// A's face -> A is reference and B is incidence.

			getAABBClippingPlanes(a.radius, conjugate(a.rotation) * normal, clipPlanePoints, clipPlaneNormals);
			getAABBIncidentVertices(b.radius, conjugate(b.rotation) * normal, polygon);

			for (uint32 i = 0; i < 4; ++i)
			{
				clipPlanePoints[i] = a.rotation * clipPlanePoints[i] + a.center;
				clipPlaneNormals[i] = a.rotation * clipPlaneNormals[i];
				polygon.points[i].vertex = b.rotation * polygon.points[i].vertex + b.center;
			}

			obb_support_fn support{ a };
			vec3 referencePlanePoint = support(normal);
			plane = createPlane(referencePlanePoint, normal);
		}
		else
		{
			// B's face -> B is reference and A is incidence.

			getAABBClippingPlanes(b.radius, conjugate(b.rotation) * -normal, clipPlanePoints, clipPlaneNormals);
			getAABBIncidentVertices(a.radius, conjugate(a.rotation) * -normal, polygon);

			for (uint32 i = 0; i < 4; ++i)
			{
				clipPlanePoints[i] = b.rotation * clipPlanePoints[i] + b.center;
				clipPlaneNormals[i] = b.rotation * clipPlaneNormals[i];
				polygon.points[i].vertex = a.rotation * polygon.points[i].vertex + a.center;
			}

			obb_support_fn support{ b };
			vec3 referencePlanePoint = support(-normal);
			plane = createPlane(referencePlanePoint, -normal);
		}

		vec4 clipPlanes[4];
		for (uint32 i = 0; i < 4; ++i)
		{
			clipPlanes[i] = createPlane(clipPlanePoints[i], clipPlaneNormals[i]);
			polygon.points[i].penetrationDepth = -signedDistanceToPlane(polygon.points[i].vertex, plane);
		}

		clipping_polygon clippedPolygon;
		sutherlandHodgmanClipping(polygon, clipPlanes, 4, clippedPolygon);

		if (clippedPolygon.numPoints == 0)
		{
			return false;
		}

		// Project onto plane and remove everything below plane.
		for (uint32 i = 0; i < clippedPolygon.numPoints; ++i)
		{
			if (clippedPolygon.points[i].penetrationDepth < 0.f)
			{
				clippedPolygon.points[i] = clippedPolygon.points[clippedPolygon.numPoints - 1];
				--clippedPolygon.numPoints;
				--i;
			}
			else
			{
				clippedPolygon.points[i].vertex += plane.xyz * clippedPolygon.points[i].penetrationDepth;
			}
		}

		if (clippedPolygon.numPoints == 0)
		{
			return false;
		}

		findStableContactManifold(clippedPolygon.points, clippedPolygon.numPoints, normal, outContact.collisionTangent, outContact);
	}
	else
	{
		//std::cout << "EDGE SHIT " << normal << "\n";

		vec3 a0, a1, b0, b1;
		getAABBIncidentEdge(a.radius, conjugate(a.rotation) * normal, a0, a1);
		getAABBIncidentEdge(b.radius, conjugate(b.rotation) * -normal, b0, b1);

		a0 = a.rotation * a0 + a.center;
		a1 = a.rotation * a1 + a.center;
		b0 = b.rotation * b0 + b.center;
		b1 = b.rotation * b1 + b.center;

		debugSphere(a0, 0.1f, greenMaterial);
		debugSphere(a1, 0.1f, greenMaterial);

		debugSphere(b0, 0.1f, redMaterial);
		debugSphere(b1, 0.1f, redMaterial);

		vec3 pa, pb;
		float sqDistance = closestPoint_SegmentSegment(line_segment{ a0, a1 }, line_segment{ b0, b1 }, pa, pb);

		outContact.numContacts = 1;
		outContact.contacts[0].penetrationDepth = sqrt(sqDistance);
		outContact.contacts[0].point = (pa + pb) * 0.5f;
	}


	for (uint32 i = 0; i < outContact.numContacts; ++i)
	{
		debugSphere(outContact.contacts[i].point, 0.1f, blueMaterial);
	}

	//std::cout << "Intersection\n";
	//std::cout << minPenetration << "     " << normal << '\n';

	return true;
}

uint32 narrowphase(collider_union* worldSpaceColliders, rigid_body_global_state* rbs, broadphase_collision* possibleCollisions, uint32 numPossibleCollisions, float dt,
	collision_constraint* outCollisionConstraints)
{
	uint32 numContacts = 0;

	for (uint32 i = 0; i < numPossibleCollisions; ++i)
	{
		broadphase_collision overlap = possibleCollisions[i];
		collider_union* colliderAInitial = worldSpaceColliders + overlap.colliderA;
		collider_union* colliderBInitial = worldSpaceColliders + overlap.colliderB;

		if (colliderAInitial->rigidBodyIndex != colliderBInitial->rigidBodyIndex)
		{
			contact_manifold& contact = outCollisionConstraints[numContacts].contact;
			bool collides = false;

			collider_union* colliderA = (colliderAInitial->type < colliderBInitial->type) ? colliderAInitial : colliderBInitial;
			collider_union* colliderB = (colliderAInitial->type < colliderBInitial->type) ? colliderBInitial : colliderAInitial;


			// Sphere tests.
			if (colliderA->type == collider_type_sphere && colliderB->type == collider_type_sphere)
			{
				collides = intersection(colliderA->sphere, colliderB->sphere, contact);
			}
			else if (colliderA->type == collider_type_sphere && colliderB->type == collider_type_capsule)
			{
				collides = intersection(colliderA->sphere, colliderB->capsule, contact);
			}
			else if (colliderA->type == collider_type_sphere && colliderB->type == collider_type_aabb)
			{
				collides = intersection(colliderA->sphere, colliderB->aabb, contact);
			}
			else if (colliderA->type == collider_type_sphere && colliderB->type == collider_type_obb)
			{
				collides = intersection(colliderA->sphere, colliderB->obb, contact);
			}
			
			// Capsule tests.
			else if (colliderA->type == collider_type_capsule && colliderB->type == collider_type_capsule)
			{
				collides = intersection(colliderA->capsule, colliderB->capsule, contact);
			}
			else if (colliderA->type == collider_type_capsule && colliderB->type == collider_type_aabb)
			{
				collides = intersection(colliderA->capsule, colliderB->aabb, contact);
			}
			else if (colliderA->type == collider_type_capsule && colliderB->type == collider_type_obb)
			{
				collides = intersection(colliderA->capsule, colliderB->obb, contact);
			}

			// AABB tests.
			else if (colliderA->type == collider_type_aabb && colliderB->type == collider_type_aabb)
			{
				collides = intersection(colliderA->aabb, colliderB->aabb, contact);
			}
			else if (colliderA->type == collider_type_aabb && colliderB->type == collider_type_obb)
			{
				collides = intersection(colliderA->aabb, colliderB->obb, contact);
			}

			// OBB tests.
			else if (colliderA->type == collider_type_obb && colliderB->type == collider_type_obb)
			{
				collides = intersection(colliderA->obb, colliderB->obb, contact);
			}



			if (collides)
			{
				collider_properties propsA = colliderA->properties;
				collider_properties propsB = colliderB->properties;

				float friction = sqrt(propsA.friction * propsB.friction);

				contact.colliderA = overlap.colliderA;
				contact.colliderB = overlap.colliderB;

				collision_constraint& c = outCollisionConstraints[numContacts];
				c.friction = friction;
				c.rbA = colliderA->rigidBodyIndex;
				c.rbB = colliderB->rigidBodyIndex;


				for (uint32 contactID = 0; contactID < contact.numContacts; ++contactID)
				{
					collision_point& point = c.points[contactID];
					contact_info& contact = c.contact.contacts[contactID];

					point.impulseInNormalDir = 0.f;
					point.impulseInTangentDir = 0.f;

					auto& rbA = rbs[c.rbA];
					auto& rbB = rbs[c.rbB];

					point.relGlobalAnchorA = contact.point - rbA.position;
					point.relGlobalAnchorB = contact.point - rbB.position;

					vec3 anchorVelocityA = rbA.linearVelocity + cross(rbA.angularVelocity, point.relGlobalAnchorA);
					vec3 anchorVelocityB = rbB.linearVelocity + cross(rbB.angularVelocity, point.relGlobalAnchorB);

					vec3 relVelocity = anchorVelocityB - anchorVelocityA;
					point.tangent = relVelocity - dot(c.contact.collisionNormal, relVelocity) * c.contact.collisionNormal;
					if (squaredLength(point.tangent) > 0.f)
					{
						point.tangent = normalize(point.tangent);
					}
					else
					{
						point.tangent = vec3(1.f, 0.f, 0.f);
					}

					{ // Tangent direction.
						vec3 crAt = cross(point.relGlobalAnchorA, point.tangent);
						vec3 crBt = cross(point.relGlobalAnchorB, point.tangent);
						float invMassInTangentDir = rbA.invMass + dot(crAt, rbA.invInertia * crAt)
							+ rbB.invMass + dot(crBt, rbB.invMass * crBt);
						point.effectiveMassInTangentDir = (invMassInTangentDir != 0.f) ? (1.f / invMassInTangentDir) : 0.f;
					}

					{ // Normal direction.
						vec3 crAn = cross(point.relGlobalAnchorA, c.contact.collisionNormal);
						vec3 crBn = cross(point.relGlobalAnchorB, c.contact.collisionNormal);
						float invMassInNormalDir = rbA.invMass + dot(crAn, rbA.invInertia * crAn)
							+ rbB.invMass + dot(crBn, rbB.invMass * crBn);
						point.effectiveMassInNormalDir = (invMassInNormalDir != 0.f) ? (1.f / invMassInNormalDir) : 0.f;

						point.bias = 0.f;

						if (dt > 0.f)
						{
							float vRel = dot(c.contact.collisionNormal, anchorVelocityB - anchorVelocityA);
							const float slop = -0.001f;
							if (-contact.penetrationDepth < slop && vRel < 0.f)
							{
								float restitution = max(propsA.restitution, propsB.restitution);
								point.bias = -restitution * vRel - 0.1f * (-contact.penetrationDepth - slop) / dt;
							}
						}
					}
				}


				++numContacts;
			}
		}
	}

	return numContacts;
}

void solveCollisionVelocityConstraints(collision_constraint* constraints, uint32 count, rigid_body_global_state* rbs)
{
	for (uint32 i = 0; i < count; ++i)
	{
		collision_constraint& c = constraints[i];

		auto& rbA = rbs[c.rbA];
		auto& rbB = rbs[c.rbB];

		vec3 vA = rbA.linearVelocity;
		vec3 wA = rbA.angularVelocity;
		vec3 vB = rbB.linearVelocity;
		vec3 wB = rbB.angularVelocity;

		for (uint32 contactID = 0; contactID < c.contact.numContacts; ++contactID)
		{
			collision_point& point = c.points[contactID];
			contact_info& contact = c.contact.contacts[contactID];

			{ // Tangent dir
				vec3 anchorVelocityA = vA + cross(wA, point.relGlobalAnchorA);
				vec3 anchorVelocityB = vB + cross(wB, point.relGlobalAnchorB);

				vec3 relVelocity = anchorVelocityB - anchorVelocityA;
				float vt = dot(relVelocity, point.tangent);
				float lambda = -point.effectiveMassInTangentDir * vt;

				float maxFriction = c.friction * point.impulseInNormalDir;
				assert(maxFriction >= 0.f);
				float newImpulse = clamp(point.impulseInTangentDir + lambda, -maxFriction, maxFriction);
				lambda = newImpulse - point.impulseInTangentDir;
				point.impulseInTangentDir = newImpulse;

				vec3 P = lambda * point.tangent;
				vA -= rbA.invMass * P;
				wA -= rbA.invInertia * cross(point.relGlobalAnchorA, P);
				vB += rbB.invMass * P;
				wB += rbB.invInertia * cross(point.relGlobalAnchorB, P);
			}

			{ // Normal dir
				vec3 anchorVelocityA = vA + cross(wA, point.relGlobalAnchorA);
				vec3 anchorVelocityB = vB + cross(wB, point.relGlobalAnchorB);

				vec3 relVelocity = anchorVelocityB - anchorVelocityA;
				float vn = dot(relVelocity, c.contact.collisionNormal);
				float lambda = -point.effectiveMassInNormalDir * (vn - point.bias);
				float impulse = max(point.impulseInNormalDir + lambda, 0.f);
				lambda = impulse - point.impulseInNormalDir;
				point.impulseInNormalDir = impulse;

				vec3 P = lambda * c.contact.collisionNormal;
				vA -= rbA.invMass * P;
				wA -= rbA.invInertia * cross(point.relGlobalAnchorA, P);
				vB += rbB.invMass * P;
				wB += rbB.invInertia * cross(point.relGlobalAnchorB, P);
			}
		}

		rbA.linearVelocity = vA;
		rbA.angularVelocity = wA;
		rbB.linearVelocity = vB;
		rbB.angularVelocity = wB;
	}
}

void initializeCollisionDebugDraw()
{
	cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals | mesh_creation_flags_with_tangents);
	sphereMesh = primitiveMesh.pushSphere(15, 15, 1.f);

	debugMesh = primitiveMesh.createDXMesh();

	std::string empty;
	redMaterial = createPBRMaterial(empty, empty, empty, empty, vec4(1.f, 0.f, 0.f, 1.f), vec4(0.f, 0.f, 0.f, 1.f));
	greenMaterial = createPBRMaterial(empty, empty, empty, empty, vec4(0.f, 1.f, 0.f, 1.f), vec4(0.f, 0.f, 0.f, 1.f));
	blueMaterial = createPBRMaterial(empty, empty, empty, empty, vec4(0.f, 0.f, 1.f, 1.f), vec4(0.f, 0.f, 0.f, 1.f));
}

void collisionDebugDraw(transparent_render_pass* renderPass)
{
	for (auto& dc : debugDrawCalls)
	{
		renderPass->renderObject(debugMesh.vertexBuffer, debugMesh.indexBuffer, dc.submesh, dc.material, dc.transform);
	}

	debugDrawCalls.clear();
}
