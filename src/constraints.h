#pragma once

#include "math.h"
#include "scene.h"

enum constraint_type : uint16
{
	constraint_type_distance,
	constraint_type_ball_joint,
	constraint_type_hinge_joint,
};

#define INVALID_CONSTRAINT_EDGE UINT16_MAX

struct constraint_edge
{
	uint16 constraint;
	constraint_type type;
	uint16 prevConstraintEdge;
	uint16 nextConstraintEdge;
};





struct physics_constraint
{
	entt::entity entityA = entt::null;
	entt::entity entityB = entt::null;
	uint16 edgeA = INVALID_CONSTRAINT_EDGE;
	uint16 edgeB = INVALID_CONSTRAINT_EDGE;
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
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;

	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;

	vec3 u;
	float bias;
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
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;
	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;

	vec3 bias;
	mat3 invEffectiveMass;
};


// Hinge-joint constraint.

struct hinge_joint_constraint : physics_constraint
{
	vec3 localAnchorA;
	vec3 localAnchorB;
	vec3 localAxisA;
	vec3 localAxisB;

	// Limits. The rotation limits are the allowed deviations in radians from the initial relative rotation.
	float minRotationLimit; // [-2pi, 0]
	float maxRotationLimit; // [0, 2pi]
	quat initialRelativeRotation;
};

struct hinge_joint_constraint_update
{
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;

	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;

	vec3 translationBias;
	mat3 invEffectiveTranslationMass;

	vec2 rotationBias;
	mat2 invEffectiveRotationMass;
	vec3 bxa;
	vec3 cxa;

	vec3 globalRotationAxis;
	float effectiveLimitMass; // Same for min and max limit.

	// Since at a single time, only one limit constraint can be violated, we only store this stuff once. 'limitSign' is positive for min- and negative for max-limit-violations.
	float limitImpulse;
	float limitBias;
	bool solveLimit;
	float limitSign;
};


struct rigid_body_global_state;

void initializeDistanceVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const distance_constraint* input, distance_constraint_update* output, uint32 count, float dt);
void solveDistanceVelocityConstraints(distance_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeBallJointVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const ball_joint_constraint* input, ball_joint_constraint_update* output, uint32 count, float dt);
void solveBallJointVelocityConstraints(ball_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeHingeJointVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const hinge_joint_constraint* input, hinge_joint_constraint_update* output, uint32 count, float dt);
void solveHingeJointVelocityConstraints(hinge_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);
