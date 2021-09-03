#pragma once

#include "core/math.h"


struct collider_union;
struct rigid_body_global_state;
struct broadphase_collision;

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

struct force_field_interaction
{
	uint16 rigidBodyIndex;
	uint16 forceFieldIndex;
};

struct narrowphase_result
{
	uint32 numCollisions;
	uint32 numForceFieldInteractions;
};

narrowphase_result narrowphase(collider_union* worldSpaceColliders, broadphase_collision* possibleCollisions, uint32 numPossibleCollisions,
	collision_constraint* outCollisionConstraints, force_field_interaction* outForceFieldInteractions);

void finalizeCollisionVelocityConstraintInitialization(collider_union* worldSpaceColliders, rigid_body_global_state* rbs,
	collision_constraint* collisionConstraints, uint32 numCollisionConstraints, float dt);

void solveCollisionVelocityConstraints(collision_constraint* constraints, uint32 count, rigid_body_global_state* rbs);
