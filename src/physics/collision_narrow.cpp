#include "pch.h"
#include "collision_narrow.h"
#include "physics.h"
#include "collision_broad.h"
#include "collision_gjk.h"
#include "collision_epa.h"
#include "collision_sat.h"
#include "core/cpu_profiling.h"


#ifndef PHYSICS_ONLY
#include "rendering/render_pass.h"
#include "rendering/debug_visualization.h"
#include "rendering/pbr.h"

struct debug_draw_call
{
	mat4 transform;
	vec4 color;
};

static std::vector<debug_draw_call> debugDrawCalls;

static void debugSphere(vec3 position, float radius, vec4 color)
{
	debugDrawCalls.push_back({
		createModelMatrix(position, quat::identity, radius),
		color,
	});
}
#endif


struct contact_manifold
{
	contact_info contacts[4];

	vec3 collisionNormal; // From a to b.
	uint32 numContacts;

	uint16 colliderA;
	uint16 colliderB;
};

static bool intersection(const bounding_oriented_box& a, const bounding_oriented_box& b, contact_manifold& outContact);

struct vertex_penetration_pair
{
	vec3 vertex;
	float penetrationDepth;
};

static void findStableContactManifold(vertex_penetration_pair* vertices, uint32 numVertices, vec3 normal, contact_manifold& outContact)
{
	// http://media.steampowered.com/apps/valve/2015/DirkGregorius_Contacts.pdf slide 103ff.

	if (numVertices > 4)
	{
		// Find first vertex along some fixed distance.
		vec3 searchDir = getTangent(normal);
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
	vertex_penetration_pair points[16];
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
static void sutherlandHodgmanClipping(clipping_polygon& input, const vec4* clipPlanes, uint32 numClipPlanes, clipping_polygon& output)
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

static vec4 getAABBReferencePlane(const bounding_box& b, vec3 normal)
{
	vec3 point(
		(normal.x < 0.f) ? b.minCorner.x : b.maxCorner.x,
		(normal.y < 0.f) ? b.minCorner.y : b.maxCorner.y,
		(normal.z < 0.f) ? b.minCorner.z : b.maxCorner.z
	);
	return createPlane(point, normal);
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

// Expects that normal and tangents are already set in contact.
static bool clipPointsAndBuildContact(clipping_polygon& polygon, const vec4* clipPlanes, uint32 numClipPlanes, const vec4& referencePlane, contact_manifold& outContact)
{
	clipping_polygon clippedPolygon;
	sutherlandHodgmanClipping(polygon, clipPlanes, numClipPlanes, clippedPolygon);

	if (clippedPolygon.numPoints > 0)
	{
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
				clippedPolygon.points[i].vertex += referencePlane.xyz * clippedPolygon.points[i].penetrationDepth;
			}
		}

		if (clippedPolygon.numPoints > 0)
		{
			findStableContactManifold(clippedPolygon.points, clippedPolygon.numPoints, outContact.collisionNormal, outContact);
			return true;
		}
	}

	return false;
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
		outContact.contacts[0].point = o.rotation * (outContact.contacts[0].point - o.center) + o.center;
		return true;
	}
	return false;
}

static bool intersection(const bounding_sphere& s, const bounding_hull& h, contact_manifold& outContact)
{
	sphere_support_fn sphereSupport{ s };
	hull_support_fn hullSupport{ h };

	gjk_simplex gjkSimplex;
	if (!gjkIntersectionTest(sphereSupport, hullSupport, gjkSimplex))
	{
		return false;
	}

	epa_result epa;
	auto epaSuccess = epaCollisionInfo(gjkSimplex, sphereSupport, hullSupport, epa);
	if (epaSuccess != epa_success)
	{
		//return false;
	}

	outContact.collisionNormal = epa.normal;
	outContact.numContacts = 1;
	outContact.contacts[0].penetrationDepth = epa.penetrationDepth;
	outContact.contacts[0].point = epa.point;

	return true;
}

// Capsule tests.
static bool intersection(const bounding_capsule& a, const bounding_capsule& b, contact_manifold& outContact)
{
	vec3 aDir = a.positionB - a.positionA;
	vec3 bDir = normalize(b.positionB - b.positionA);

	float aDirLength = length(aDir);
	aDir *= 1.f / aDirLength;

	float parallel = dot(aDir, bDir);
	if (abs(parallel) > 0.99f)
	{
		// Parallel case.

		vec3 pAa = a.positionA;
		vec3 pAb = a.positionB;
		vec3 pBa = b.positionA;
		vec3 pBb = b.positionB;

		if (parallel < 0.f)
		{
			std::swap(pBa, pBb);
		}

		vec3 referencePoint = a.positionA;

		float a0 = 0.f;
		float a1 = aDirLength;

		float b0 = dot(aDir, pBa - referencePoint);
		float b1 = dot(aDir, pBb - referencePoint);
		assert(b1 > b0);

		float left = max(a0, b0);
		float right = min(a1, b1);

		if (right < left)
		{
			if (a0 > b1)
			{
				return intersection(bounding_sphere{ pAa, a.radius }, bounding_sphere{ pBb, b.radius }, outContact);
			}
			else
			{
				return intersection(bounding_sphere{ pAb, a.radius }, bounding_sphere{ pBa, b.radius }, outContact);
			}
		}

		vec3 contactA0 = referencePoint + left * aDir;
		vec3 contactA1 = referencePoint + right * aDir;

		vec3 contactB0 = closestPoint_PointSegment(contactA0, line_segment{ pBa, pBb });
		vec3 contactB1 = contactB0 + (right - left) * aDir;

		vec3 normal = contactB0 - contactA0;
		float d = length(normal);

		if (d < EPSILON)
		{
			d = 0.f;
			normal = vec3(0.f, 1.f, 0.f);
		}
		else
		{
			normal /= d;
		}

		float radiusSum = a.radius + b.radius;

		float penetration = radiusSum - d;
		if (penetration < 0.f)
		{
			return false;
		}

		outContact.collisionNormal = normal;
		outContact.numContacts = 2;
		outContact.contacts[0].penetrationDepth = penetration;
		outContact.contacts[0].point = (contactA0 + contactB0) * 0.5f;
		outContact.contacts[1].penetrationDepth = penetration;
		outContact.contacts[1].point = (contactA1 + contactB1) * 0.5f;

		return true;
	}
	else
	{
		vec3 closestPoint1, closestPoint2;
		closestPoint_SegmentSegment(line_segment{ a.positionA, a.positionB }, line_segment{ b.positionA, b.positionB }, closestPoint1, closestPoint2);
		return intersection(bounding_sphere{ closestPoint1, a.radius }, bounding_sphere{ closestPoint2, b.radius }, outContact);
	}
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

	vec3 normal = epa.normal;
	vec3 point = epa.point;

	outContact.collisionNormal = normal;
	outContact.numContacts = 1;
	outContact.contacts[0].penetrationDepth = epa.penetrationDepth;
	outContact.contacts[0].point = point;

	if (abs(normal.x) > 0.99f || abs(normal.y) > 0.99f || abs(normal.z) > 0.99f)
	{
		// Probably AABB face.

		vec3 axis = normalize(c.positionB - c.positionA);
		if (abs(dot(normal, axis)) < 0.01f)
		{
			// Capsule is parallel to AABB face (perpendicular to normal).

			vec3 clipPlanePoints[4];
			vec3 clipPlaneNormals[4];
			vec4 clipPlanes[4];

			vec3 aabbNormal = -normal;

			vec4 referencePlane = getAABBReferencePlane(a, aabbNormal);

			clipping_polygon polygon;
			polygon.numPoints = 2;
			vec3 pa = c.positionA + normal * c.radius;
			vec3 pb = c.positionB + normal * c.radius;
			polygon.points[0] = { pa, -signedDistanceToPlane(pa, referencePlane) };
			polygon.points[1] = { pb, -signedDistanceToPlane(pb, referencePlane) };

			vec3 aCenter = a.getCenter();
			getAABBClippingPlanes(a.getRadius(), aabbNormal, clipPlanePoints, clipPlaneNormals);
			for (uint32 i = 0; i < 4; ++i)
			{
				clipPlanePoints[i] = clipPlanePoints[i] + aCenter;
				clipPlaneNormals[i] = clipPlaneNormals[i];
				clipPlanes[i] = createPlane(clipPlanePoints[i], clipPlaneNormals[i]);
			}

			clipPointsAndBuildContact(polygon, clipPlanes, 4, referencePlane, outContact);
		}
	}

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

		for (uint32 i = 0; i < outContact.numContacts; ++i)
		{
			outContact.contacts[i].point = o.rotation * (outContact.contacts[i].point - o.center) + o.center;
		}
		return true;
	}
	return false;
}

static bool intersection(const bounding_capsule& c, const bounding_hull& h, contact_manifold& outContact)
{
	// TODO: Handle multiple-contact-points case.

	capsule_support_fn capsuleSupport{ c };
	hull_support_fn hullSupport{ h };

	gjk_simplex gjkSimplex;
	if (!gjkIntersectionTest(capsuleSupport, hullSupport, gjkSimplex))
	{
		return false;
	}

	epa_result epa;
	auto epaSuccess = epaCollisionInfo(gjkSimplex, capsuleSupport, hullSupport, epa);
	if (epaSuccess != epa_success)
	{
		//return false;
	}

	outContact.collisionNormal = epa.normal;
	outContact.numContacts = 1;
	outContact.contacts[0].penetrationDepth = epa.penetrationDepth;
	outContact.contacts[0].point = epa.point;

	return true;
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

	//debugSphere(outContact.contacts[0].point, 0.1f, { 0.f, 0.f, 1.f, 0.f });
	//debugSphere(outContact.contacts[1].point, 0.1f, { 0.f, 0.f, 1.f, 0.f });
	//debugSphere(outContact.contacts[2].point, 0.1f, { 0.f, 0.f, 1.f, 0.f });
	//debugSphere(outContact.contacts[3].point, 0.1f, { 0.f, 0.f, 1.f, 0.f });

	return true;
}

static bool intersection(const bounding_box& a, const bounding_oriented_box& b, contact_manifold& outContact)
{
	// We forward to the more general case OBB vs OBB here. This is not ideal, since this test then again transforms to a space local
	// to one OOB.
	// However, I don't expect this function to be called very often, as AABBs are uncommon, so this is probably fine.
	return intersection(bounding_oriented_box{ a.getCenter(), a.getRadius(), quat::identity }, b, outContact);
}

static bool intersection(const bounding_box& a, const bounding_hull& h, contact_manifold& outContact)
{
	// TODO: Handle multiple-contact-points case.

	aabb_support_fn aabbSupport{ a };
	hull_support_fn hullSupport{ h };

	gjk_simplex gjkSimplex;
	if (!gjkIntersectionTest(aabbSupport, hullSupport, gjkSimplex))
	{
		return false;
	}

	epa_result epa;
	auto epaSuccess = epaCollisionInfo(gjkSimplex, aabbSupport, hullSupport, epa);
	if (epaSuccess != epa_success)
	{
		//return false;
	}

	outContact.collisionNormal = epa.normal;
	outContact.numContacts = 1;
	outContact.contacts[0].penetrationDepth = epa.penetrationDepth;
	outContact.contacts[0].point = epa.point;

	return true;
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

	if (faceCollision)
	{
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

		if (!clipPointsAndBuildContact(polygon, clipPlanes, 4, plane, outContact))
		{
			return false;
		}
	}
	else
	{
		vec3 a0, a1, b0, b1;
		getAABBIncidentEdge(a.radius, conjugate(a.rotation) * normal, a0, a1);
		getAABBIncidentEdge(b.radius, conjugate(b.rotation) * -normal, b0, b1);

		a0 = a.rotation * a0 + a.center;
		a1 = a.rotation * a1 + a.center;
		b0 = b.rotation * b0 + b.center;
		b1 = b.rotation * b1 + b.center;

		//debugSphere(a0, 0.1f, { 0.f, 1.f, 0.f, 1.f });
		//debugSphere(a1, 0.1f, { 0.f, 1.f, 0.f, 1.f });
		//debugSphere(b0, 0.1f, { 1.f, 0.f, 0.f, 1.f });
		//debugSphere(b1, 0.1f, { 1.f, 0.f, 0.f, 1.f });

		vec3 pa, pb;
		float sqDistance = closestPoint_SegmentSegment(line_segment{ a0, a1 }, line_segment{ b0, b1 }, pa, pb);

		outContact.numContacts = 1;
		outContact.contacts[0].penetrationDepth = sqrt(sqDistance);
		outContact.contacts[0].point = (pa + pb) * 0.5f;
	}


	//for (uint32 i = 0; i < outContact.numContacts; ++i)
	//{
	//	debugSphere(outContact.contacts[i].point, 0.1f, { 0.f, 0.f, 1.f, 0.f });
	//}

	return true;
}

static bool intersection(const bounding_oriented_box& o, const bounding_hull& h, contact_manifold& outContact)
{
	// TODO: Handle multiple-contact-points case.

	obb_support_fn obbSupport{ o };
	hull_support_fn hullSupport{ h };

	gjk_simplex gjkSimplex;
	if (!gjkIntersectionTest(obbSupport, hullSupport, gjkSimplex))
	{
		return false;
	}

	epa_result epa;
	auto epaSuccess = epaCollisionInfo(gjkSimplex, obbSupport, hullSupport, epa);
	if (epaSuccess != epa_success)
	{
		//return false;
	}

	outContact.collisionNormal = epa.normal;
	outContact.numContacts = 1;
	outContact.contacts[0].penetrationDepth = epa.penetrationDepth;
	outContact.contacts[0].point = epa.point;

	return true;
}

// Hull tests.
static bool intersection(const bounding_hull& a, const bounding_hull& b, contact_manifold& outContact)
{
	// TODO: Handle multiple-contact-points case.

	hull_support_fn hullSupport1{ a };
	hull_support_fn hullSupport2{ b };

	gjk_simplex gjkSimplex;
	if (!gjkIntersectionTest(hullSupport1, hullSupport2, gjkSimplex))
	{
		return false;
	}

	epa_result epa;
	auto epaSuccess = epaCollisionInfo(gjkSimplex, hullSupport1, hullSupport2, epa);
	if (epaSuccess != epa_success)
	{
		//return false;
	}

	outContact.collisionNormal = epa.normal;
	outContact.numContacts = 1;
	outContact.contacts[0].penetrationDepth = epa.penetrationDepth;
	outContact.contacts[0].point = epa.point;

	return true;
}

static bool collisionCheck(collider_union* worldSpaceColliders, broadphase_collision overlap, contact_manifold& contact)
{
	collider_union* colliderAInitial = worldSpaceColliders + overlap.colliderA;
	collider_union* colliderBInitial = worldSpaceColliders + overlap.colliderB;

	bool keepOrder = (colliderAInitial->type < colliderBInitial->type);
	collider_union* colliderA = keepOrder ? colliderAInitial : colliderBInitial;
	collider_union* colliderB = keepOrder ? colliderBInitial : colliderAInitial;

	bool collides = false;

	switch (colliderA->type)
	{
		// Sphere tests.
		case collider_type_sphere:
		{
			switch (colliderB->type)
			{
				case collider_type_sphere: collides = intersection(colliderA->sphere, colliderB->sphere, contact); break;
				case collider_type_capsule: collides = intersection(colliderA->sphere, colliderB->capsule, contact); break;
				case collider_type_aabb: collides = intersection(colliderA->sphere, colliderB->aabb, contact); break;
				case collider_type_obb: collides = intersection(colliderA->sphere, colliderB->obb, contact); break;
				case collider_type_hull: collides = intersection(colliderA->sphere, colliderB->hull, contact); break;
			}
		} break;

		// Capsule tests.
		case collider_type_capsule:
		{
			switch (colliderB->type)
			{
				case collider_type_capsule: collides = intersection(colliderA->capsule, colliderB->capsule, contact); break;
				case collider_type_aabb: collides = intersection(colliderA->capsule, colliderB->aabb, contact); break;
				case collider_type_obb: collides = intersection(colliderA->capsule, colliderB->obb, contact); break;
				case collider_type_hull: collides = intersection(colliderA->capsule, colliderB->hull, contact); break;
			}
		} break;

		// AABB tests.
		case collider_type_aabb:
		{
			switch (colliderB->type)
			{
				case collider_type_aabb: collides = intersection(colliderA->aabb, colliderB->aabb, contact); break;
				case collider_type_obb: collides = intersection(colliderA->aabb, colliderB->obb, contact); break;
				case collider_type_hull: collides = intersection(colliderA->aabb, colliderB->hull, contact); break;
			}
		} break;

		// OBB tests.
		case collider_type_obb:
		{
			switch (colliderB->type)
			{
				case collider_type_obb: collides = intersection(colliderA->obb, colliderB->obb, contact); break;
				case collider_type_hull: collides = intersection(colliderA->obb, colliderB->hull, contact); break;
			}
		} break;

		// Hull tests.
		case collider_type_hull:
		{
			switch (colliderB->type)
			{
				case collider_type_hull: collides = intersection(colliderA->hull, colliderB->hull, contact); break;
			}
		} break;
	}

	if (collides)
	{
		contact.colliderA = keepOrder ? overlap.colliderA : overlap.colliderB;
		contact.colliderB = keepOrder ? overlap.colliderB : overlap.colliderA;
		return true;
	}
	return false;
}

static bool overlapCheck(collider_union* worldSpaceColliders, broadphase_collision overlap, non_collision_interaction& interaction)
{
	collider_union* colliderAInitial = worldSpaceColliders + overlap.colliderA;
	collider_union* colliderBInitial = worldSpaceColliders + overlap.colliderB;

	bool keepOrder = (colliderAInitial->type < colliderBInitial->type);
	collider_union* colliderA = keepOrder ? colliderAInitial : colliderBInitial;
	collider_union* colliderB = keepOrder ? colliderBInitial : colliderAInitial;

	assert(colliderA->type == physics_object_type_rigid_body || colliderB->type == physics_object_type_rigid_body);
	assert(colliderA->type != physics_object_type_rigid_body || colliderB->type != physics_object_type_rigid_body);

	bool overlaps = false;

	switch (colliderA->type)
	{
		// Sphere tests.
		case collider_type_sphere:
		{
			switch (colliderB->type)
			{
				case collider_type_sphere: overlaps = sphereVsSphere(colliderA->sphere, colliderB->sphere); break;
				case collider_type_capsule: overlaps = sphereVsCapsule(colliderA->sphere, colliderB->capsule); break;
				case collider_type_aabb: overlaps = sphereVsAABB(colliderA->sphere, colliderB->aabb); break;
				case collider_type_obb: overlaps = sphereVsOBB(colliderA->sphere, colliderB->obb); break;
				case collider_type_hull: overlaps = sphereVsHull(colliderA->sphere, colliderB->hull); break;
			}
		} break;

		// Capsule tests.
		case collider_type_capsule:
		{
			switch (colliderB->type)
			{
				case collider_type_capsule: overlaps = capsuleVsCapsule(colliderA->capsule, colliderB->capsule); break;
				case collider_type_aabb: overlaps = capsuleVsAABB(colliderA->capsule, colliderB->aabb); break;
				case collider_type_obb: overlaps = capsuleVsOBB(colliderA->capsule, colliderB->obb); break;
				case collider_type_hull: overlaps = capsuleVsHull(colliderA->capsule, colliderB->hull); break;
			}
		} break;

		// AABB tests.
		case collider_type_aabb:
		{
			switch (colliderB->type)
			{
				case collider_type_aabb: overlaps = aabbVsAABB(colliderA->aabb, colliderB->aabb); break;
				case collider_type_obb: overlaps = aabbVsOBB(colliderA->aabb, colliderB->obb); break;
				case collider_type_hull: overlaps = aabbVsHull(colliderA->aabb, colliderB->hull); break;
			}
		} break;

		// OBB tests.
		case collider_type_obb:
		{
			switch (colliderB->type)
			{
				case collider_type_obb: overlaps = obbVsOBB(colliderA->obb, colliderB->obb); break;
				case collider_type_hull: overlaps = obbVsHull(colliderA->obb, colliderB->hull); break;
			}
		} break;

		// Hull tests.
		case collider_type_hull:
		{
			switch (colliderB->type)
			{
				case collider_type_hull: overlaps = hullVsHull(colliderA->hull, colliderB->hull); break;
			}
		} break;
	}

	if (overlaps)
	{
		if (colliderA->type == physics_object_type_rigid_body)
		{
			interaction.rigidBodyIndex = colliderA->objectIndex;
			interaction.otherIndex = colliderB->objectIndex;
			interaction.otherType = colliderB->objectType;
		}
		else
		{
			interaction.rigidBodyIndex = colliderB->objectIndex;
			interaction.otherIndex = colliderA->objectIndex;
			interaction.otherType = colliderA->objectType;
		}

		return true;
	}
	return false;
}

narrowphase_result narrowphase(collider_union* worldSpaceColliders, broadphase_collision* possibleCollisions, uint32 numPossibleCollisions,
	collision_contact* outContacts, non_collision_interaction* outNonCollisionInteractions)
{
	CPU_PROFILE_BLOCK("Narrow phase");

	uint32 numContacts = 0;
	uint32 numNonCollisionInteractions = 0;

	for (uint32 i = 0; i < numPossibleCollisions; ++i)
	{
		broadphase_collision overlap = possibleCollisions[i];
		collider_union* colliderA = worldSpaceColliders + overlap.colliderA;
		collider_union* colliderB = worldSpaceColliders + overlap.colliderB;

		if (colliderA->objectType != physics_object_type_rigid_body && colliderB->objectType != physics_object_type_rigid_body)
		{
			// If none of the objects is a rigid body, no collision is generated.
			continue;
		}

		if (colliderA->objectType == physics_object_type_rigid_body && colliderB->objectType == physics_object_type_rigid_body
			&& colliderA->objectIndex == colliderB->objectIndex)
		{
			// If both colliders belong to the same rigid body, no collision is generated.
			continue;
		}

		// At this point, either one or both colliders belong to a rigid body. One of them could be a force field or a solo collider still.

		if (colliderA->objectType == physics_object_type_force_field || colliderB->objectType == physics_object_type_force_field)
		{
			non_collision_interaction& interaction = outNonCollisionInteractions[numNonCollisionInteractions];
			numNonCollisionInteractions += overlapCheck(worldSpaceColliders, overlap, interaction);
		}
		else
		{
			contact_manifold contact;
			if (collisionCheck(worldSpaceColliders, overlap, contact))
			{
				collider_union* colliderA = worldSpaceColliders + contact.colliderA;
				collider_union* colliderB = worldSpaceColliders + contact.colliderB;

				uint16 rbA = colliderA->objectIndex;
				uint16 rbB = colliderB->objectIndex;

				collider_properties propsA = colliderA->properties;
				collider_properties propsB = colliderB->properties;

				float friction = clamp01(sqrt(propsA.friction * propsB.friction));
				float restitution = clamp01(max(propsA.restitution, propsB.restitution));

				uint32 friction_restitution = ((uint32)(friction * 0xFFFF) << 16) | (uint32)(restitution * 0xFFFF);

				for (uint32 contactIndex = 0; contactIndex < contact.numContacts; ++contactIndex)
				{
					collision_contact& out = outContacts[numContacts++];
					out.normal = contact.collisionNormal;
					out.penetrationDepth = contact.contacts[contactIndex].penetrationDepth;
					out.point = contact.contacts[contactIndex].point;
					out.rbA = rbA;
					out.rbB = rbB;
					out.friction_restitution = friction_restitution;
				}
			}
		}
	}

	return narrowphase_result{ numContacts, numNonCollisionInteractions };
}

void initializeCollisionVelocityConstraints(rigid_body_global_state* rbs, collision_contact* contacts, collision_constraint* collisionConstraints, uint32 numContacts, float dt)
{
	CPU_PROFILE_BLOCK("Initialize collision constraints");

	for (uint32 contactID = 0; contactID < numContacts; ++contactID)
	{
		collision_constraint& constraint = collisionConstraints[contactID];
		collision_contact& contact = contacts[contactID];

		auto& rbA = rbs[contact.rbA];
		auto& rbB = rbs[contact.rbB];

		constraint.impulseInNormalDir = 0.f;
		constraint.impulseInTangentDir = 0.f;

		constraint.relGlobalAnchorA = contact.point - rbA.position;
		constraint.relGlobalAnchorB = contact.point - rbB.position;

		vec3 anchorVelocityA = rbA.linearVelocity + cross(rbA.angularVelocity, constraint.relGlobalAnchorA);
		vec3 anchorVelocityB = rbB.linearVelocity + cross(rbB.angularVelocity, constraint.relGlobalAnchorB);

		vec3 relVelocity = anchorVelocityB - anchorVelocityA;
		constraint.tangent = relVelocity - dot(contact.normal, relVelocity) * contact.normal;
		constraint.tangent = noz(constraint.tangent);

		{ // Tangent direction.
			vec3 crAt = cross(constraint.relGlobalAnchorA, constraint.tangent);
			vec3 crBt = cross(constraint.relGlobalAnchorB, constraint.tangent);
			float invMassInTangentDir = rbA.invMass + dot(crAt, rbA.invInertia * crAt)
				+ rbB.invMass + dot(crBt, rbB.invInertia * crBt);
			constraint.effectiveMassInTangentDir = (invMassInTangentDir != 0.f) ? (1.f / invMassInTangentDir) : 0.f;

			constraint.tangentImpulseToAngularVelocityA = rbA.invInertia * crAt;
			constraint.tangentImpulseToAngularVelocityB = rbB.invInertia * crBt;
		}

		{ // Normal direction.
			vec3 crAn = cross(constraint.relGlobalAnchorA, contact.normal);
			vec3 crBn = cross(constraint.relGlobalAnchorB, contact.normal);
			float invMassInNormalDir = rbA.invMass + dot(crAn, rbA.invInertia * crAn)
				+ rbB.invMass + dot(crBn, rbB.invInertia * crBn);
			constraint.effectiveMassInNormalDir = (invMassInNormalDir != 0.f) ? (1.f / invMassInNormalDir) : 0.f;

			constraint.bias = 0.f;

			if (dt > 1e-5f)
			{
				float vRel = dot(contact.normal, anchorVelocityB - anchorVelocityA);
				const float slop = -0.001f;
				if (-contact.penetrationDepth < slop && vRel < 0.f)
				{
					float restitution = (float)(contact.friction_restitution & 0xFFFF) / (float)0xFFFF;
					constraint.bias = -restitution * vRel - 0.1f * (-contact.penetrationDepth - slop) / dt;
				}
			}

			constraint.normalImpulseToAngularVelocityA = rbA.invInertia * crAn;
			constraint.normalImpulseToAngularVelocityB = rbB.invInertia * crBn;
		}
	}
}

void solveCollisionVelocityConstraints(collision_contact* contacts, collision_constraint* constraints, uint32 count, rigid_body_global_state* rbs)
{
	CPU_PROFILE_BLOCK("Solve collision constraints");

	for (uint32 i = 0; i < count; ++i)
	{
		collision_contact& contact = contacts[i];
		collision_constraint& constraint = constraints[i];

		auto& rbA = rbs[contact.rbA];
		auto& rbB = rbs[contact.rbB];

		if (rbA.invMass == 0.f && rbB.invMass == 0.f)
		{
			continue;
		}

		vec3 vA = rbA.linearVelocity;
		vec3 wA = rbA.angularVelocity;
		vec3 vB = rbB.linearVelocity;
		vec3 wB = rbB.angularVelocity;

		{ // Tangent dir.
			vec3 anchorVelocityA = vA + cross(wA, constraint.relGlobalAnchorA);
			vec3 anchorVelocityB = vB + cross(wB, constraint.relGlobalAnchorB);

			vec3 relVelocity = anchorVelocityB - anchorVelocityA;
			float vt = dot(relVelocity, constraint.tangent);
			float lambda = -constraint.effectiveMassInTangentDir * vt;

			float friction = (float)(contact.friction_restitution >> 16) / (float)0xFFFF;
			float maxFriction = friction * constraint.impulseInNormalDir;
			assert(maxFriction >= 0.f);
			float newImpulse = clamp(constraint.impulseInTangentDir + lambda, -maxFriction, maxFriction);
			lambda = newImpulse - constraint.impulseInTangentDir;
			constraint.impulseInTangentDir = newImpulse;

			vec3 P = lambda * constraint.tangent;
			vA -= rbA.invMass * P;
			wA -= constraint.tangentImpulseToAngularVelocityA * lambda;
			vB += rbB.invMass * P;
			wB += constraint.tangentImpulseToAngularVelocityB * lambda;
		}

		{ // Normal dir.
			vec3 anchorVelocityA = vA + cross(wA, constraint.relGlobalAnchorA);
			vec3 anchorVelocityB = vB + cross(wB, constraint.relGlobalAnchorB);

			vec3 relVelocity = anchorVelocityB - anchorVelocityA;
			float vn = dot(relVelocity, contact.normal);
			float lambda = -constraint.effectiveMassInNormalDir * (vn - constraint.bias);
			float impulse = max(constraint.impulseInNormalDir + lambda, 0.f);
			lambda = impulse - constraint.impulseInNormalDir;
			constraint.impulseInNormalDir = impulse;

			vec3 P = lambda * contact.normal;
			vA -= rbA.invMass * P;
			wA -= constraint.normalImpulseToAngularVelocityA * lambda;
			vB += rbB.invMass * P;
			wB += constraint.normalImpulseToAngularVelocityB * lambda;
		}

		rbA.linearVelocity = vA;
		rbA.angularVelocity = wA;
		rbB.linearVelocity = vB;
		rbB.angularVelocity = wB;
	}
}

#include "core/math_simd.h"

#if COLLISION_SIMD_WIDTH == 4
typedef vec2x<floatx4> vec2w;
typedef vec3x<floatx4> vec3w;
typedef vec4x<floatx4> vec4w;
typedef quatx<floatx4> quatw;
typedef mat3x<floatx4> mat3w;
typedef floatx4 floatw;
typedef intx4 intw;
#else
typedef vec2x<floatx8> vec2w;
typedef vec3x<floatx8> vec3w;
typedef vec4x<floatx8> vec4w;
typedef quatx<floatx8> quatw;
typedef mat3x<floatx8> mat3w;
typedef floatx8 floatw;
typedef intx8 intw;
#endif

void initializeCollisionVelocityConstraintsSIMD(memory_arena& arena, rigid_body_global_state* rbs, collision_contact* contacts, uint32 numContacts,
	uint16 dummyRigidBodyIndex, simd_collision_constraint& outConstraints, float dt, simd_collision_metrics& metrics)
{
	CPU_PROFILE_BLOCK("Initialize collision constraints SIMD");

	struct alignas(32) simd_contact_pair
	{
		uint32 ab[COLLISION_SIMD_WIDTH];
	};

	struct alignas(32) simd_contact_slot
	{
		uint32 indices[COLLISION_SIMD_WIDTH];
	};


	simd_contact_slot* contactSlots = arena.allocate<simd_contact_slot>(numContacts);
	uint32 numContactSlots = 0;


	static const uint32 numBuckets = 4;
	simd_contact_pair* pairBuckets[numBuckets];
	simd_contact_slot* slotBuckets[numBuckets];
	uint32 numEntriesPerBucket[numBuckets] = {};

	intw invalid = ~0;


	for (unsigned i = 0; i < numBuckets; ++i)
	{
		pairBuckets[i] = arena.allocate<simd_contact_pair>(numContacts + 1);
		slotBuckets[i] = arena.allocate<simd_contact_slot>(numContacts);

		// Add padding with invalid data so we don't have to range check.
		invalid.store((int*)pairBuckets[i]->ab);
	}

	for (uint32 i = 0; i < numContacts; ++i) 
	{
		collision_contact& contact = contacts[i];

		// If one of the bodies is the dummy, just set it to the other for the comparison below.
		uint16 rbA = (contact.rbA == dummyRigidBodyIndex) ? contact.rbB : contact.rbA;
		uint16 rbB = (contact.rbB == dummyRigidBodyIndex) ? contact.rbA : contact.rbB;

		uint32 bucket = i % numBuckets;
		simd_contact_pair* pairs = pairBuckets[bucket];
		simd_contact_slot* slots = slotBuckets[bucket];


#if COLLISION_SIMD_WIDTH == 4
		intw a = _mm_set1_epi16(rbA);
		intw b = _mm_set1_epi16(rbB);
		intw scheduled;

		uint32 j = 0;
		for (;; ++j)
		{
			scheduled = _mm_load_si128((const __m128i*)pairs[j].ab);

			__m128i conflictsWithThisSlot = _mm_packs_epi16(_mm_cmpeq_epi16(a, scheduled), _mm_cmpeq_epi16(b, scheduled));
			if (!_mm_movemask_epi8(conflictsWithThisSlot))
			{
				break;
			}
		}
#else
		intw a = _mm256_set1_epi16(rbA);
		intw b = _mm256_set1_epi16(rbB);
		intw scheduled;

		uint32 j = 0;
		for (;; ++j)
		{
			scheduled = _mm256_load_si256((const __m256i*)pairs[j].ab);

			__m256i conflictsWithThisSlot = _mm256_packs_epi16(_mm256_cmpeq_epi16(a, scheduled), _mm256_cmpeq_epi16(b, scheduled));
			if (!_mm256_movemask_epi8(conflictsWithThisSlot))
			{
				break;
			}
		}

#endif


		uint32 lane = indexOfLeastSignificantSetBit(toBitMask(reinterpret(scheduled == invalid)));

		simd_contact_pair* pair = pairs + j;
		simd_contact_slot* slot = slots + j;

		slot->indices[lane] = i;
		pair->ab[lane] = ((uint32)contact.rbA << 16) | contact.rbB; // Use the original indices here.

		uint32& count = numEntriesPerBucket[bucket];
		if (j == count)
		{
			// We used a new entry.
			++count;

			// Set entry at end to invalid.
			invalid.store((int*)pairs[count].ab);
		}
		else if (lane == COLLISION_SIMD_WIDTH - 1)
		{
			// This entry is full -> commit it.
			intw indices = (int32*)slot->indices;

			// Swap and pop.
			--count;
			*pair = pairs[count];
			*slot = slots[count];

			indices.store((int32*)contactSlots[numContactSlots++].indices);

			// Set entry at end to invalid.
			invalid.store((int*)pairs[count].ab);
		}
	}

	// There are still entries left, where not all lanes are filled. We replace these indices with the first in this entry.

	for (uint32 bucket = 0; bucket < numBuckets; ++bucket)
	{
		simd_contact_pair* pairs = pairBuckets[bucket];
		simd_contact_slot* slots = slotBuckets[bucket];
		uint32 count = numEntriesPerBucket[bucket];

		for (uint32 i = 0; i < count; ++i)
		{
			intw ab = (int32*)pairs[i].ab;
			intw indices = (int32_t*)slots[i].indices;

			intw firstIndex = fillWithFirstLane(indices);

			auto mask = ab == invalid;
			indices = ifThen(mask, firstIndex, indices);
			indices.store((int32*)contactSlots[numContactSlots++].indices);
		}
	}


	metrics.simdWidth = COLLISION_SIMD_WIDTH;
	metrics.numBatches = numContactSlots;
	metrics.numContacts = numContacts;
	metrics.averageFillRate = (float)numContacts / (float)(numContactSlots * COLLISION_SIMD_WIDTH);


	// Initialize constraints.

	outConstraints.numBatches = numContactSlots;
	outConstraints.batches = arena.allocate<simd_collision_batch>(outConstraints.numBatches);

	const floatw zero = floatw::zero();
	const floatw slop = -0.001f;
	const floatw scale = 0.1f;
	const floatw invDt = 1.f / dt;

	for (uint32 i = 0; i < numContactSlots; ++i)
	{
		simd_contact_slot& slot = contactSlots[i];
		simd_collision_batch& batch = outConstraints.batches[i];

		uint16 contactIndices[COLLISION_SIMD_WIDTH];
		for (uint32 j = 0; j < COLLISION_SIMD_WIDTH; ++j)
		{
			contactIndices[j] = (uint16)slot.indices[j];
			batch.rbAIndices[j] = contacts[slot.indices[j]].rbA;
			batch.rbBIndices[j] = contacts[slot.indices[j]].rbB;
		}

		vec3w point, normal;
		floatw penetrationDepth, friction_restitutionF;
		load8((float*)contacts, contactIndices, (uint32)sizeof(collision_contact),
			point.x, point.y, point.z, penetrationDepth, normal.x, normal.y, normal.z, friction_restitutionF);
		intw friction_restitution = reinterpret(friction_restitutionF);

		floatw friction = convert(friction_restitution >> 16) / floatw(0xFFFF);
		floatw restitution = convert(friction_restitution & 0xFFFF) / floatw(0xFFFF);


		// Load body A.
		vec3w vA, wA;
		mat3w invInertiaA;
		floatw invMassA;
		vec3w positionA;
		floatw unused;

		load8((float*)&rbs->linearVelocity, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			vA.x, vA.y, vA.z, wA.x, wA.y, wA.z, invMassA, invInertiaA.m00);

		load8((float*)&rbs->invInertia.m10, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			invInertiaA.m10, invInertiaA.m20, invInertiaA.m01, invInertiaA.m11, 
			invInertiaA.m21, invInertiaA.m02, invInertiaA.m12, invInertiaA.m22);

		load4((float*)&rbs->position, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			positionA.x, positionA.y, positionA.z, unused);


		// Load body B.
		vec3w vB, wB;
		mat3w invInertiaB;
		floatw invMassB;
		vec3w positionB;

		load8((float*)&rbs->linearVelocity, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			vB.x, vB.y, vB.z, wB.x, wB.y, wB.z, invMassB, invInertiaB.m00);

		load8((float*)&rbs->invInertia.m10, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			invInertiaB.m10, invInertiaB.m20, invInertiaB.m01, invInertiaB.m11, 
			invInertiaB.m21, invInertiaB.m02, invInertiaB.m12, invInertiaB.m22);

		load4((float*)&rbs->position, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			positionB.x, positionB.y, positionB.z, unused);





		vec3w relGlobalAnchorA = point - positionA;
		vec3w relGlobalAnchorB = point - positionB;

		vec3w anchorVelocityA = vA + cross(wA, relGlobalAnchorA);
		vec3w anchorVelocityB = vB + cross(wB, relGlobalAnchorB);

		vec3w relVelocity = anchorVelocityB - anchorVelocityA;
		vec3w tangent = relVelocity - dot(normal, relVelocity) * normal;
		tangent = noz(tangent);


		relGlobalAnchorA.x.store(batch.relGlobalAnchorA[0]);
		relGlobalAnchorA.y.store(batch.relGlobalAnchorA[1]);
		relGlobalAnchorA.z.store(batch.relGlobalAnchorA[2]);

		relGlobalAnchorB.x.store(batch.relGlobalAnchorB[0]);
		relGlobalAnchorB.y.store(batch.relGlobalAnchorB[1]);
		relGlobalAnchorB.z.store(batch.relGlobalAnchorB[2]);

		normal.x.store(batch.normal[0]);
		normal.y.store(batch.normal[1]);
		normal.z.store(batch.normal[2]);

		tangent.x.store(batch.tangent[0]);
		tangent.y.store(batch.tangent[1]);
		tangent.z.store(batch.tangent[2]);

		zero.store(batch.impulseInNormalDir);
		zero.store(batch.impulseInTangentDir);
		friction.store(batch.friction);



		{ // Tangent direction.
			vec3w crAt = cross(relGlobalAnchorA, tangent);
			vec3w crBt = cross(relGlobalAnchorB, tangent);
			floatw invMassInTangentDir = invMassA + dot(crAt, invInertiaA * crAt)
									   + invMassB + dot(crBt, invInertiaB * crBt);
			floatw effectiveMassInTangentDir = ifThen(invMassInTangentDir != zero, 1.f / invMassInTangentDir, zero);
			effectiveMassInTangentDir.store(batch.effectiveMassInTangentDir);

			vec3w tangentImpulseToAngularVelocityA = invInertiaA * crAt;
			tangentImpulseToAngularVelocityA.x.store(batch.tangentImpulseToAngularVelocityA[0]);
			tangentImpulseToAngularVelocityA.y.store(batch.tangentImpulseToAngularVelocityA[1]);
			tangentImpulseToAngularVelocityA.z.store(batch.tangentImpulseToAngularVelocityA[2]);

			vec3w tangentImpulseToAngularVelocityB = invInertiaB * crBt;
			tangentImpulseToAngularVelocityB.x.store(batch.tangentImpulseToAngularVelocityB[0]);
			tangentImpulseToAngularVelocityB.y.store(batch.tangentImpulseToAngularVelocityB[1]);
			tangentImpulseToAngularVelocityB.z.store(batch.tangentImpulseToAngularVelocityB[2]);
		}
		

		{ // Normal direction.
			vec3w crAn = cross(relGlobalAnchorA, normal);
			vec3w crBn = cross(relGlobalAnchorB, normal);
			floatw invMassInNormalDir = invMassA + dot(crAn, invInertiaA * crAn)
									  + invMassB + dot(crBn, invInertiaB * crBn);
			floatw effectiveMassInNormalDir = ifThen(invMassInNormalDir != zero, 1.f / invMassInNormalDir, zero);

			floatw bias = zero;

			if (dt > 1e-5f)
			{
				floatw vRel = dot(normal, anchorVelocityB - anchorVelocityA);

				floatw bounceBias = -restitution * vRel - scale * (-penetrationDepth - slop) * invDt;
				bias = ifThen((-penetrationDepth < slop) & (vRel < zero), bounceBias, bias);
			}

			effectiveMassInNormalDir.store(batch.effectiveMassInNormalDir);
			bias.store(batch.bias);

			vec3w normalImpulseToAngularVelocityA = invInertiaA * crAn;
			normalImpulseToAngularVelocityA.x.store(batch.normalImpulseToAngularVelocityA[0]);
			normalImpulseToAngularVelocityA.y.store(batch.normalImpulseToAngularVelocityA[1]);
			normalImpulseToAngularVelocityA.z.store(batch.normalImpulseToAngularVelocityA[2]);

			vec3w normalImpulseToAngularVelocityB = invInertiaB * crBn;
			normalImpulseToAngularVelocityB.x.store(batch.normalImpulseToAngularVelocityB[0]);
			normalImpulseToAngularVelocityB.y.store(batch.normalImpulseToAngularVelocityB[1]);
			normalImpulseToAngularVelocityB.z.store(batch.normalImpulseToAngularVelocityB[2]);
		}
	}

}

void solveCollisionVelocityConstraintsSIMD(simd_collision_constraint& constraints, rigid_body_global_state* rbs)
{
	CPU_PROFILE_BLOCK("Solve collision constraints SIMD");

	for (uint32 i = 0; i < constraints.numBatches; ++i)
	{
		simd_collision_batch& batch = constraints.batches[i];

		// Load body A.
		vec3w vA, wA;
		floatw invMassA;
		floatw dummyA;

		load8((float*)&rbs->linearVelocity, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			vA.x, vA.y, vA.z, wA.x, wA.y, wA.z, invMassA, dummyA);


		// Load body B.
		vec3w vB, wB;
		floatw invMassB;
		floatw dummyB;

		load8((float*)&rbs->linearVelocity, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			vB.x, vB.y, vB.z, wB.x, wB.y, wB.z, invMassB, dummyB);


		// Load constraint.
		vec3w relGlobalAnchorA(batch.relGlobalAnchorA[0], batch.relGlobalAnchorA[1], batch.relGlobalAnchorA[2]);
		vec3w relGlobalAnchorB(batch.relGlobalAnchorB[0], batch.relGlobalAnchorB[1], batch.relGlobalAnchorB[2]);
		vec3w normal(batch.normal[0], batch.normal[1], batch.normal[2]);
		vec3w tangent(batch.tangent[0], batch.tangent[1], batch.tangent[2]);
		floatw effectiveMassInNormalDir(batch.effectiveMassInNormalDir);
		floatw effectiveMassInTangentDir(batch.effectiveMassInTangentDir);
		floatw friction(batch.friction);
		floatw impulseInNormalDir(batch.impulseInNormalDir);
		floatw impulseInTangentDir(batch.impulseInTangentDir);
		floatw bias(batch.bias);

		vec3w tangentImpulseToAngularVelocityA(batch.tangentImpulseToAngularVelocityA[0], batch.tangentImpulseToAngularVelocityA[1], batch.tangentImpulseToAngularVelocityA[2]);
		vec3w tangentImpulseToAngularVelocityB(batch.tangentImpulseToAngularVelocityB[0], batch.tangentImpulseToAngularVelocityB[1], batch.tangentImpulseToAngularVelocityB[2]);

		{ // Tangent dir.
			vec3w anchorVelocityA = vA + cross(wA, relGlobalAnchorA);
			vec3w anchorVelocityB = vB + cross(wB, relGlobalAnchorB);

			vec3w relVelocity = anchorVelocityB - anchorVelocityA;
			floatw vt = dot(relVelocity, tangent);
			floatw lambda = -effectiveMassInTangentDir * vt;

			floatw maxFriction = friction * impulseInNormalDir;
			floatw newImpulse = clamp(impulseInTangentDir + lambda, -maxFriction, maxFriction);
			lambda = newImpulse - impulseInTangentDir;
			impulseInTangentDir = newImpulse;

			vec3w P = lambda * tangent;
			vA -= invMassA * P;
			wA -= tangentImpulseToAngularVelocityA * lambda;
			vB += invMassB * P;
			wB += tangentImpulseToAngularVelocityB * lambda;
		}

		vec3w normalImpulseToAngularVelocityA(batch.normalImpulseToAngularVelocityA[0], batch.normalImpulseToAngularVelocityA[1], batch.normalImpulseToAngularVelocityA[2]);
		vec3w normalImpulseToAngularVelocityB(batch.normalImpulseToAngularVelocityB[0], batch.normalImpulseToAngularVelocityB[1], batch.normalImpulseToAngularVelocityB[2]);

		{ // Normal dir.
			vec3w anchorVelocityA = vA + cross(wA, relGlobalAnchorA);
			vec3w anchorVelocityB = vB + cross(wB, relGlobalAnchorB);

			vec3w relVelocity = anchorVelocityB - anchorVelocityA;
			floatw vn = dot(relVelocity, normal);
			floatw lambda = -effectiveMassInNormalDir * (vn - bias);
			floatw impulse = maximum(impulseInNormalDir + lambda, floatw::zero());
			lambda = impulse - impulseInNormalDir;
			impulseInNormalDir = impulse;

			vec3w P = lambda * normal;
			vA -= invMassA * P;
			wA -= normalImpulseToAngularVelocityA * lambda;
			vB += invMassB * P;
			wB += normalImpulseToAngularVelocityB * lambda;
		}

		impulseInNormalDir.store(batch.impulseInNormalDir);
		impulseInTangentDir.store(batch.impulseInTangentDir);

		store8((float*)&rbs->linearVelocity, batch.rbAIndices, (uint32)sizeof(rigid_body_global_state),
			vA.x, vA.y, vA.z, wA.x, wA.y, wA.z, invMassA, dummyA);

		store8((float*)&rbs->linearVelocity, batch.rbBIndices, (uint32)sizeof(rigid_body_global_state),
			vB.x, vB.y, vB.z, wB.x, wB.y, wB.z, invMassB, dummyB);
	}
}

#ifndef PHYSICS_ONLY
void collisionDebugDraw(ldr_render_pass* renderPass)
{
	static dx_mesh debugMesh;
	static submesh_info sphereMesh;

	if (!debugMesh.vertexBuffer.positions)
	{
		cpu_mesh primitiveMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals);
		sphereMesh = primitiveMesh.pushSphere(15, 15, 1.f);
		debugMesh = primitiveMesh.createDXMesh();		
	}

	for (auto& dc : debugDrawCalls)
	{
		renderPass->renderObject<debug_unlit_pipeline>(dc.transform, debugMesh.vertexBuffer, debugMesh.indexBuffer, sphereMesh, { dc.color });
	}
	
	debugDrawCalls.clear();
}
#endif
