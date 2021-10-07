#pragma once

#include "core/math.h"
#include "physics.h"

struct collider_union;
struct broadphase_collision;

struct contact_info
{
	vec3 point;
	float penetrationDepth;
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
	uint32 numContacts;
	uint32 numNonCollisionInteractions;
};

narrowphase_result narrowphase(collider_union* worldSpaceColliders, broadphase_collision* possibleCollisions, uint32 numPossibleCollisions,
	collision_contact* outContacts, constraint_body_pair* outBodyPairs, non_collision_interaction* outNonCollisionInteractions);


