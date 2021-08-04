#pragma once

#include "core/math.h"


struct contact_info
{
	vec3 point; // In world space.
	float penetrationDepth;
};

struct contact_manifold
{
	uint16 colliderA;
	uint16 colliderB;

	contact_info contacts[4];

	vec3 collisionNormal; // From a to b.
	vec3 collisionTangent;
	vec3 collisionBitangent;
	uint32 numContacts;
};

struct collision_point
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

struct collision_constraint
{
	contact_manifold contact;

	collision_point points[4];

	uint16 rbA;
	uint16 rbB;

	float friction;
};

uint32 narrowphase(struct collider_union* worldSpaceColliders, struct rigid_body_global_state* rbs, struct broadphase_collision* possibleCollisions, uint32 numPossibleCollisions, float dt,
	collision_constraint* outCollisionConstraints);

void solveCollisionVelocityConstraints(collision_constraint* constraints, uint32 count, rigid_body_global_state* rbs);
