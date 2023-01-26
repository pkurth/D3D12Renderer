#include "pch.h"
#include "heightmap_collision.h"
#include "core/cpu_profiling.h"


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
			case collider_type_sphere:
			{
				numContacts = intersection(collider.sphere, aabb, heightmap, arena, contactPtr);
			} break;
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
			//outContactCountPerCollision[totalNumCollisions] = (uint8)numContacts;
			//outColliderPairs[totalNumCollisions++] = { (uint16)i, 0 }; // TODO
		}


		totalNumContacts += numContacts;
	}

	return narrowphase_result{ totalNumCollisions, totalNumContacts, 0 };
}
