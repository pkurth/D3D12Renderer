#pragma once

#include "core/math.h"
#include "physics.h"

struct collider_union;
struct collider_pair;

struct non_collision_interaction
{
	uint16 rigidBodyIndex;
	uint16 otherIndex;
	physics_object_type otherType;
};

struct narrowphase_result
{
	uint32 numCollisions;
	uint32 numContacts;						// Number of contacts between colliders. Each collision (see above) may have up to 4 contacts for a stable contact.
	uint32 numNonCollisionInteractions;		// Number of interactions between RBs and triggers, force fields etc.
};

// outColliderPairs may be the same as colliderPairs
narrowphase_result narrowphase(const collider_union* worldSpaceColliders, collider_pair* colliderPairs, uint32 numCollisionPairs, memory_arena& arena,
	collision_contact* outContacts, constraint_body_pair* outBodyPairs, // result.numContacts many.
	collider_pair* outColliderPairs, uint8* outContactCountPerCollision, // result.numCollisions many.
	non_collision_interaction* outNonCollisionInteractions,			// result.numNonCollisionInteractions many.
	bool simd);

