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

	for (uint32 clipIndex = 0; clipIndex < numClipPlanes; ++clipIndex)
	{
		vec4 clipPlane = clipPlanes[clipIndex];
		out->numPoints = 0;

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

	if (numClipPlanes % 2 == 0)
	{
		for (uint32 i = 0; i < input.numPoints; ++i)
		{
			output.points[i] = input.points[i];
		}
		output.numPoints = input.numPoints;
	}
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
	vec4 aabbPlanes[] =
	{
		createPlane(a.minCorner, vec3(-1.f, 0.f, 0.f)),
		createPlane(a.minCorner, vec3(0.f, -1.f, 0.f)),
		createPlane(a.minCorner, vec3(0.f, 0.f, -1.f)),
		createPlane(a.maxCorner, vec3(1.f, 0.f, 0.f)),
		createPlane(a.maxCorner, vec3(0.f, 1.f, 0.f)),
		createPlane(a.maxCorner, vec3(0.f, 0.f, 1.f)),
	};

	aabb_support_fn aabbSupport{ a };
	obb_support_fn obbSupport{ b };

	sat_result sat;
	sat_status first = satIntersectionTest(aabbPlanes, arraysize(aabbPlanes), obbSupport, sat);
	if (first == sat_no_intersection)
	{
		return false;
	}

	bool aabbFace = true;

	vec3 obbMinCorner = b.center - b.rotation * b.radius;
	vec3 obbMaxCorner = b.center + b.rotation * b.radius;

	vec3 obbAxisX = b.rotation * vec3(1.f, 0.f, 0.f);
	vec3 obbAxisY = b.rotation * vec3(0.f, 1.f, 0.f);
	vec3 obbAxisZ = b.rotation * vec3(0.f, 0.f, 1.f);

	vec4 obbPlanes[] =
	{
		createPlane(obbMinCorner, -obbAxisX),
		createPlane(obbMinCorner, -obbAxisY),
		createPlane(obbMinCorner, -obbAxisZ),
		createPlane(obbMaxCorner, obbAxisX),
		createPlane(obbMaxCorner, obbAxisY),
		createPlane(obbMaxCorner, obbAxisZ),
	};

	sat_status second = satIntersectionTest(obbPlanes, arraysize(obbPlanes), aabbSupport, sat);
	if (second == sat_no_intersection)
	{
		return false;
	}
	if (second == sat_better_intersection)
	{
		aabbFace = false;
	}

	bool faceContact = true;

	bounding_box_corners aabbCorners = a.getCorners();
	bounding_box_corners obbCorners = b.getCorners();

#if 0
	for (uint32 i = 0; i < 8; ++i)
	{
		obbCorners.corners[i] = b.rotation * (obbCorners.corners[i] - b.center) + b.center;
	}

	static const indexed_line32 edgeIndices[] =
	{
		{ 0, 1 },
		{ 0, 4 },
		{ 1, 5 },
		{ 4, 5 },
		{ 0, 2 },
		{ 1, 3 },
		{ 4, 6 },
		{ 5, 7 },
		{ 2, 3 },
		{ 2, 6 },
		{ 6, 7 },
		{ 3, 7 },
	};

	struct edge
	{
		vec3 start;
		vec3 direction;
	};

	edge aabbEdges[12];
	edge obbEdges[12];

	for (uint32 i = 0; i < 12; ++i)
	{
		aabbEdges[i].start = aabbCorners.corners[edgeIndices[i].a];
		aabbEdges[i].direction = aabbCorners.corners[edgeIndices[i].b] - aabbEdges[i].start;

		obbEdges[i].start = obbCorners.corners[edgeIndices[i].a];
		obbEdges[i].direction = obbCorners.corners[edgeIndices[i].b] - obbEdges[i].start;
	}


	aabb_support_fn aabbSupport{ a };
	for (uint32 aEdgeIndex = 0; aEdgeIndex < 12; ++aEdgeIndex)
	{
		for (uint32 bEdgeIndex = 0; bEdgeIndex < 12; ++bEdgeIndex)
		{
			vec3 axis = cross(aabbEdges[aEdgeIndex].direction, obbEdges[bEdgeIndex].direction);
			if (dot(axis, obbEdges[bEdgeIndex].start - b.center) < 0.f)
			{
				axis = -axis;
			}

			vec4 plane = createPlane(aabbEdges[bEdgeIndex].start, axis);
			vec3 normal = plane.xyz;

			vec3 farthest = aabbSupport(-normal);

			float sd = signedDistanceToPlane(farthest, plane);
			if (sd > 0.f)
			{
				return false;
			}

			float penetration = -sd;
			if (penetration < sat.minPenetration)
			{
				sat.minPenetration = penetration;
				sat.normal = normal;
				sat.faceIndex = (aEdgeIndex << 16) | bEdgeIndex;
				faceContact = false;
			}
		}
	}
#endif

	if (faceContact)
	{
		vec3 normal = sat.plane.xyz;
		outContact.collisionNormal = aabbFace ? normal : -normal;

		getTangents(outContact.collisionNormal, outContact.collisionTangent, outContact.collisionBitangent);

		auto getClippingPlanes = [](const bounding_box& aabb, vec3 normal, vec3* clipPlanePoints, vec3* clipPlaneNormals)
		{
			vec3 p = abs(normal);

			uint32 maxElement = (p.x > p.y) ? ((p.x > p.z) ? 0 : 2) : ((p.y > p.z) ? 1 : 2);

			uint32 axis0 = (maxElement + 1) % 3;
			uint32 axis1 = (maxElement + 2) % 3;

			{
				vec3 planeNormal(0.f); planeNormal.data[axis0] = 1.f;
				clipPlaneNormals[0] = planeNormal;
				clipPlanePoints[0] = aabb.minCorner;
			}
			{
				vec3 planeNormal(0.f); planeNormal.data[axis1] = 1.f;
				clipPlaneNormals[1] = planeNormal;
				clipPlanePoints[1] = aabb.minCorner;
			}
			{
				vec3 planeNormal(0.f); planeNormal.data[axis0] = -1.f;
				clipPlaneNormals[2] = planeNormal;
				clipPlanePoints[2] = aabb.maxCorner;
			}
			{
				vec3 planeNormal(0.f); planeNormal.data[axis1] = -1.f;
				clipPlaneNormals[3] = planeNormal;
				clipPlanePoints[3] = aabb.maxCorner;
			}
		};
		auto getIncidentVertices = [](const bounding_box& aabb, vec3 normal, clipping_polygon& polygon)
		{
			vec3 p = abs(normal);
			vec3 radius = aabb.getRadius();

			uint32 maxElement = (p.x > p.y) ? ((p.x > p.z) ? 0 : 2) : ((p.y > p.z) ? 1 : 2);
			float s = normal.data[maxElement] < 0.f ? 1.f : -1.f; // Flipped sign.

			uint32 axis0 = (maxElement + 1) % 3;
			uint32 axis1 = (maxElement + 2) % 3;

			float d = radius.data[maxElement] * s;
			float min0 = -radius.data[axis0];
			float min1 = -radius.data[axis1];
			float max0 = radius.data[axis0];
			float max1 = radius.data[axis1];

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
		};

		vec3 clipPlanePoints[4];
		vec3 clipPlaneNormals[4];

		clipping_polygon polygon;

		bounding_box obbLocal = bounding_box::fromCenterRadius(0.f, b.radius);

		if (aabbFace)
		{
			getClippingPlanes(a, normal, clipPlanePoints, clipPlaneNormals);
			getIncidentVertices(obbLocal, conjugate(b.rotation) * normal, polygon);

			for (uint32 i = 0; i < 4; ++i)
			{
				polygon.points[i].vertex = b.rotation * polygon.points[i].vertex + b.center;
			}
		}
		else
		{
			getClippingPlanes(obbLocal, conjugate(b.rotation) * normal, clipPlanePoints, clipPlaneNormals);
			getIncidentVertices(a, normal, polygon);

			for (uint32 i = 0; i < 4; ++i)
			{
				clipPlanePoints[i] = b.rotation * clipPlanePoints[i] + b.center;
				clipPlaneNormals[i] = b.rotation * clipPlaneNormals[i];
			}
		}
		
		vec4 clipPlanes[4];
		for (uint32 i = 0; i < 4; ++i)
		{
			clipPlanes[i] = createPlane(clipPlanePoints[i], clipPlaneNormals[i]);
			polygon.points[i].penetrationDepth = -signedDistanceToPlane(polygon.points[i].vertex, sat.plane);
		}

		clipping_polygon clippedPolygon;
		sutherlandHodgmanClipping(polygon, clipPlanes, 4, clippedPolygon);

		assert(clippedPolygon.numPoints > 0);

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
				clippedPolygon.points[i].vertex += sat.plane.xyz * clippedPolygon.points[i].penetrationDepth;
			}
		}

		if (clippedPolygon.numPoints == 0)
		{
			return false;
		}

		findStableContactManifold(clippedPolygon.points, clippedPolygon.numPoints, normal, outContact.collisionTangent, outContact);

		for (uint32 i = 0; i < outContact.numContacts; ++i)
		{
			debugSphere(outContact.contacts[i].point, 0.1f, blueMaterial);
		}

		return true;
	}

	return false; // TODO: Edge contact.
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
