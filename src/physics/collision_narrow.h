#pragma once

#include "core/math.h"
#include "physics.h"

struct collider_union;
struct collider_pair;

struct contact_info
{
	vec3 point;
	float penetrationDepth; // Positive.
};

struct collision_contact
{
	// Don't change the order here.
	vec3 point;
	float penetrationDepth;
	vec3 normal;
	uint32 friction_restitution; // Packed as 16 bit int each. The packing makes it more convenient for the SIMD code to load the contact data.
};

struct non_collision_interaction
{
	uint16 rigidBodyIndex;
	uint16 otherIndex;
	physics_object_type otherType;
};

struct narrowphase_result
{
	uint32 numCollisions;					// Number of RB-RB collisions. If the bodies have multiple colliders, there may be multiple collisions per pair.
	uint32 numContacts;						// Number of contacts between colliders. Each collision (see above) may have up to 4 contacts for a stable contact.
	uint32 numNonCollisionInteractions;		// Number of interactions between RBs and triggers, force fields etc.
};

// After this function returns, the first result.numCollisions entries of collisionPairs indicate the RB-RB collisions, which actually passed the narrow phase.
narrowphase_result narrowphase(const collider_union* worldSpaceColliders, collider_pair* collisionPairs, uint32 numCollisionPairs,
	collision_contact* outContacts, constraint_body_pair* outBodyPairs, // result.numContacts many.
	non_collision_interaction* outNonCollisionInteractions);			// result.numNonCollisionInteractions many.


