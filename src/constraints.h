#pragma once

#include "math.h"
#include "scene.h"

enum constraint_type : uint16
{
	constraint_type_distance,
	constraint_type_ball_joint,
};

#define INVALID_CONSTRAINT_EDGE UINT16_MAX

struct physics_constraint
{
	entt::entity entityA = entt::null;
	entt::entity entityB = entt::null;
	uint16 edgeA = INVALID_CONSTRAINT_EDGE;
	uint16 edgeB = INVALID_CONSTRAINT_EDGE;
};

struct constraint_edge
{
	uint16 constraint;
	constraint_type type;
	uint16 prevConstraintEdge;
	uint16 nextConstraintEdge;
};


// Distance constraint.

struct distance_constraint : physics_constraint
{
	vec3 localAnchorA;
	vec3 localAnchorB;
	float globalLength;
};

struct distance_constraint_update
{
	vec3 u;
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;
	vec3 relGlobalAnchorA;
	float bias;
	vec3 relGlobalAnchorB;
	float effectiveMass;
};


// Ball-joint constraint.

struct ball_joint_constraint : physics_constraint
{
	vec3 localAnchorA;
	vec3 localAnchorB;
};

struct ball_joint_constraint_update
{
	vec3 bias;
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;
	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;
	mat3 invEffectiveMass;
};


struct rigid_body_global_state;

void initializeDistanceConstraints(scene& appScene, rigid_body_global_state* rbs, const distance_constraint* input, distance_constraint_update* output, uint32 count, float dt);
void solveDistanceConstraints(distance_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeBallJointConstraints(scene& appScene, rigid_body_global_state* rbs, const ball_joint_constraint* input, ball_joint_constraint_update* output, uint32 count, float dt);
void solveBallJointConstraints(ball_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);
