#pragma once

#include "core/math.h"
#include "physics.h"

struct collider_union;
struct rigid_body_global_state;
struct broadphase_collision;

struct contact_info
{
	vec3 point;
	float penetrationDepth;
};

struct collision_contact
{
	vec3 point;
	float penetrationDepth;
	vec3 normal;
	float friction;
	float restitution;
	uint16 rbA;
	uint16 rbB;
};

struct collision_constraint
{
	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;
	vec3 tangent;

	float impulseInNormalDir;
	float impulseInTangentDir;
	float effectiveMassInNormalDir;
	float effectiveMassInTangentDir;
	float bias;
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
	collision_contact* outContacts, non_collision_interaction* outNonCollisionInteractions);

void initializeCollisionVelocityConstraints(rigid_body_global_state* rbs, collision_contact* contacts, collision_constraint* collisionConstraints, uint32 numContacts, float dt);

void solveCollisionVelocityConstraints(collision_contact* contacts, collision_constraint* constraints, uint32 count, rigid_body_global_state* rbs);
