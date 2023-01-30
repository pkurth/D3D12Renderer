#include "pch.h"
#include "heightmap_collision.h"
#include "core/cpu_profiling.h"

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


static uint32 collideAABBvsTriangle(vec3 center, vec3 radius, vec3 a, vec3 b, vec3 c, collision_contact* outContacts)
{
	a -= center;
	b -= center;
	c -= center;

	vec3 f0 = b - a;
	vec3 f1 = c - b;
	vec3 f2 = a - c;

	// See aabbVsTriangle for more detailed comments.

	float minPenetration = FLT_MAX;
	vec3 minNormal;

	enum intersection_category
	{
		category0_edge0,
		category0_edge1,
		category0_edge2,

		category1,
		category2,
	};

	intersection_category category = category0_edge0;

	{
		float p0 = (a.z * f0.y) - (a.y * f0.z);
		float p2 = (c.z * f0.y) - (c.y * f0.z);
		float r = radius.y * abs(f0.z) + radius.z * abs(f0.y);

		float penetration = r - max(-max(p0, p2), min(p0, p2));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(0.f, -f0.z, f0.y);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge0;
		}
	}

	{
		float p0 = (a.z * f1.y) - (a.y * f1.z);
		float p1 = (b.z * f1.y) - (b.y * f1.z);
		float r = radius.y * abs(f1.z) + radius.z * abs(f1.y);

		float penetration = r - max(-max(p0, p1), min(p0, p1));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(0.f, -f1.z, f1.y);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge1;
		}
	}

	{
		float p0 = (a.z * f2.y) - (a.y * f2.z);
		float p1 = (b.z * f2.y) - (b.y * f2.z);
		float r = radius.y * abs(f2.z) + radius.z * abs(f2.y);

		float penetration = r - max(-max(p0, p1), min(p0, p1));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(0.f, -f2.z, f2.y);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge2;
		}
	}


	{
		float p0 = (a.x * f0.z) - (a.z * f0.x);
		float p2 = (c.x * f0.z) - (c.z * f0.x);
		float r = radius.x * abs(f0.z) + radius.z * abs(f0.x);

		float penetration = r - max(-max(p0, p2), min(p0, p2));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(f0.z, 0.f, -f0.x);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge0;
		}
	}

	{
		float p0 = (a.x * f1.z) - (a.z * f1.x);
		float p1 = (b.x * f1.z) - (b.z * f1.x);
		float r = radius.x * abs(f1.z) + radius.z * abs(f1.x);

		float penetration = r - max(-max(p0, p1), min(p0, p1));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(f1.z, 0.f, -f1.x);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge1;
		}
	}

	{
		float p0 = (a.x * f2.z) - (a.z * f2.x);
		float p1 = (b.x * f2.z) - (b.z * f2.x);
		float r = radius.x * abs(f2.z) + radius.z * abs(f2.x);

		float penetration = r - max(-max(p0, p1), min(p0, p1));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(f2.z, 0.f, -f2.x);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge2;
		}
	}


	{
		float p0 = (a.y * f0.x) - (a.x * f0.y);
		float p2 = (c.y * f0.x) - (c.x * f0.y);
		float r = radius.x * abs(f0.y) + radius.y * abs(f0.x);

		float penetration = r - max(-max(p0, p2), min(p0, p2));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(-f0.y, f0.x, 0.f);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge0;
		}
	}

	{
		float p0 = (a.y * f1.x) - (a.x * f1.y);
		float p1 = (b.y * f1.x) - (b.x * f1.y);
		float r = radius.x * abs(f1.y) + radius.y * abs(f1.x);

		float penetration = r - max(-max(p0, p1), min(p0, p1));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(-f1.y, f1.x, 0.f);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge1;
		}
	}

	{
		float p0 = (a.y * f2.x) - (a.x * f2.y);
		float p1 = (b.y * f2.x) - (b.x * f2.y);
		float r = radius.x * abs(f2.y) + radius.y * abs(f2.x);

		float penetration = r - max(-max(p0, p1), min(p0, p1));
		if (penetration < 0.f) return 0;

		vec3 normal = vec3(-f2.y, f2.x, 0.f);
		float l = length(normal);
		penetration *= 1.f / l;
		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = normal * (1.f / l);
			category = category0_edge2;
		}
	}


	{
		float penetration = max(a.x, max(b.x, c.x)) + radius.x;
		if (penetration < 0.f) return 0;

		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = vec3(-1.f, 0.f, 0.f);
			category = category1;
		}
	}
	{
		float penetration = radius.x - min(a.x, min(b.x, c.x));
		if (penetration < 0.f) return 0;

		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = vec3(1.f, 0.f, 0.f);
			category = category1;
		}
	}
	{
		float penetration = max(a.y, max(b.y, c.y)) + radius.y;
		if (penetration < 0.f) return 0;

		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = vec3(0.f, -1.f, 0.f);
			category = category1;
		}
	}
	{
		float penetration = radius.y - min(a.y, min(b.y, c.y));
		if (penetration < 0.f) return 0;

		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = vec3(0.f, 1.f, 0.f);
			category = category1;
		}
	}
	{
		float penetration = max(a.z, max(b.z, c.z)) + radius.z;
		if (penetration < 0.f) return 0;

		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = vec3(0.f, 0.f, -1.f);
			category = category1;
		}
	}
	{
		float penetration = radius.z - min(a.z, min(b.z, c.z));
		if (penetration < 0.f) return 0;

		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = vec3(0.f, 0.f, 1.f);
			category = category1;
		}
	}

	{
		vec3 triNormal = normalize(cross(f0, f1));
		float triD = dot(triNormal, a);

		float r = dot(radius, abs(triNormal));
		float s = abs(triD);

		float penetration = r - s;
		if (penetration < 0.f) return 0;

		if (penetration < minPenetration)
		{
			minPenetration = penetration;
			minNormal = triNormal;
			category = category2;
		}
	}


	vec3 triCenter = (a + b + c) * (1.f / 3.f);
	if (dot(minNormal, triCenter) < 0.f)
	{
		minNormal = -minNormal;
	}


	vec3 point;

	if (category < 3)
	{
		vec3 a0, a1;
		getAABBIncidentEdge(radius, minNormal, a0, a1);

		vec3 triA = (category == category0_edge0) ? a : (category == category0_edge1) ? b : c;
		vec3 triB = (category == category0_edge0) ? b : (category == category0_edge1) ? c : a;

		vec3 pa, pb;
		float sqDistance = closestPoint_SegmentSegment(line_segment{ a0, a1 }, line_segment{ triA, triB }, pa, pb);

		point = (pa + pb) * 0.5f;
	}
	else if (category == category1)
	{
		float da = dot(minNormal, a);
		float db = dot(minNormal, b);
		float dc = dot(minNormal, c);

		vec3 p = (da < db) ? (da < dc) ? a : c : (db < dc) ? b : c;
		point = p + minNormal * (minPenetration * 0.5f);
	}
	else
	{
		vec3 p = vec3(
			(minNormal.x < 0.f) ? -radius.x : radius.x,
			(minNormal.y < 0.f) ? -radius.y : radius.y,
			(minNormal.z < 0.f) ? -radius.z : radius.z
		);

		point = p - minNormal * (minPenetration * 0.5f);
	}

	assert(minPenetration >= 0.f);

	point += center;


	collision_contact& contact = outContacts[0];
	contact.point = point;
	contact.normal = minNormal;
	contact.penetrationDepth = minPenetration;

	return 1;
}



static uint32 intersection(const bounding_sphere& s, const bounding_box& aabb, const heightmap_collider_component& heightmap, memory_arena& arena,
	collision_contact* outContacts)
{
	uint32 numContacts = 0;

	heightmap.iterateTrianglesInVolume(aabb, arena, [&s, outContacts, &numContacts](vec3 a, vec3 b, vec3 c)
	{
		vec3 closestPoint = closestPoint_PointTriangle(s.center, a, b, c);
		vec3 n = closestPoint - s.center;

		float sqLength = squaredLength(n);
		if (sqLength <= s.radius * s.radius)
		{
			float distance = sqrt(sqLength);
			n *= 1.f / distance;

			float penetrationDepth = s.radius - distance;
			assert(penetrationDepth >= 0.f);

			collision_contact& c = outContacts[numContacts++];
			c.point = closestPoint;
			c.normal = n;
			c.penetrationDepth = penetrationDepth;
		}
	});

	// TODO: De-duplicate contacts (for if we hit triangle edges or vertices).

	return numContacts;
}

static uint32 intersection(const bounding_box& aabb, const heightmap_collider_component& heightmap, memory_arena& arena,
	collision_contact* outContacts)
{
	uint32 numContacts = 0;

	vec3 center = aabb.getCenter();
	vec3 radius = aabb.getRadius();

	heightmap.iterateTrianglesInVolume(aabb, arena, [center, radius, outContacts, &numContacts](vec3 a, vec3 b, vec3 c)
	{
		numContacts += collideAABBvsTriangle(center, radius, a, b, c, outContacts);
	});

	// TODO: De-duplicate contacts (for if we hit triangle edges or vertices).

	return numContacts;
}

static uint32 intersection(const bounding_oriented_box& obb, const bounding_box& aabb, const heightmap_collider_component& heightmap, memory_arena& arena,
	collision_contact* outContacts)
{
	uint32 numContacts = 0;

	heightmap.iterateTrianglesInVolume(aabb, arena, [&obb, outContacts, &numContacts](vec3 a, vec3 b, vec3 c)
	{
		a = conjugate(obb.rotation) * (a - obb.center);
		b = conjugate(obb.rotation) * (b - obb.center);
		c = conjugate(obb.rotation) * (c - obb.center);

		numContacts += collideAABBvsTriangle(vec3(0.f, 0.f, 0.f), obb.radius, a, b, c, outContacts + numContacts);
	});

	// TODO: De-duplicate contacts (for if we hit triangle edges or vertices).

	for (uint32 i = 0; i < numContacts; ++i)
	{
		outContacts[i].normal = obb.rotation * outContacts[i].normal;
		outContacts[i].point = obb.rotation * outContacts[i].point + obb.center;
	}

	return numContacts;
}

narrowphase_result heightmapCollision(const heightmap_collider_component& heightmap, 
	const collider_union* worldSpaceColliders, const bounding_box* worldSpaceAABBs, uint32 numColliders, 
	collision_contact* outContacts, constraint_body_pair* outBodyPairs, collider_pair* outColliderPairs, uint8* outContactCountPerCollision, 
	memory_arena& arena, uint16 dummyRigidBodyIndex)
{
	CPU_PROFILE_BLOCK("Heightmap collisions");

	uint32 totalNumContacts = 0;
	uint32 totalNumCollisions = 0;

	for (uint32 i = 0; i < numColliders; ++i)
	{
		const collider_union& collider = worldSpaceColliders[i];
		const bounding_box& aabb = worldSpaceAABBs[i];

		uint32 numContacts = 0;

		collision_contact* contactPtr = outContacts + totalNumContacts;

		switch (collider.type)
		{
			case collider_type_sphere: numContacts = intersection(collider.sphere, aabb, heightmap, arena, contactPtr); break;
			case collider_type_aabb: numContacts = intersection(collider.aabb, heightmap, arena, contactPtr); break;
			case collider_type_obb: numContacts = intersection(collider.obb, aabb, heightmap, arena, contactPtr); break;
		}


		if (numContacts > 0)
		{
			constraint_body_pair* bodyPairPtr = outBodyPairs + totalNumContacts;

			float friction = clamp01(sqrt(collider.material.friction * heightmap.material.friction));
			float restitution = clamp01(max(collider.material.restitution, heightmap.material.restitution));

			uint32 friction_restitution = ((uint32)(friction * 0xFFFF) << 16) | (uint32)(restitution * 0xFFFF);

			for (uint32 j = 0; j < numContacts; ++j)
			{
				contactPtr[j].friction_restitution = friction_restitution;
				bodyPairPtr[j] = { collider.objectIndex, dummyRigidBodyIndex };
			}

			assert(numContacts < 256);
			outContactCountPerCollision[totalNumCollisions] = (uint8)numContacts;
			outColliderPairs[totalNumCollisions++] = { (uint16)i, UINT16_MAX };
		}


		totalNumContacts += numContacts;
	}

	return narrowphase_result{ totalNumCollisions, totalNumContacts, 0 };
}
