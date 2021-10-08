#pragma once

#include "core/math.h"
#include "core/memory.h"
#include "scene/scene.h"


#define CONSTRAINT_SIMD_WIDTH 8

enum constraint_type
{
	constraint_type_none = -1,

	constraint_type_distance,
	constraint_type_ball_joint,
	constraint_type_hinge_joint,
	constraint_type_cone_twist,

	constraint_type_count,
};

#define INVALID_CONSTRAINT_EDGE UINT16_MAX

struct constraint_edge
{
	entt::entity constraintEntity;
	constraint_type type;
	uint16 prevConstraintEdge;
	uint16 nextConstraintEdge;
};


enum constraint_motor_type
{
	constraint_velocity_motor,
	constraint_position_motor,
};

static const char* constraintMotorTypeNames[] =
{
	"Velocity",
	"Position",
};

struct constraint_body_pair
{
	uint16 rbA, rbB;
};


struct constraint_entity_reference_component
{
	entt::entity entityA = entt::null;
	entt::entity entityB = entt::null;
	uint16 edgeA = INVALID_CONSTRAINT_EDGE;
	uint16 edgeB = INVALID_CONSTRAINT_EDGE;
};


// Distance constraint.

struct distance_constraint
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

	vec3 impulseToAngularVelocityA;
	vec3 impulseToAngularVelocityB;

	vec3 u;
	float bias;
	float effectiveMass;
};

struct simd_distance_constraint_batch
{
	float relGlobalAnchorA[3][CONSTRAINT_SIMD_WIDTH];
	float relGlobalAnchorB[3][CONSTRAINT_SIMD_WIDTH];

	float impulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float impulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];

	float u[3][CONSTRAINT_SIMD_WIDTH];
	float bias[CONSTRAINT_SIMD_WIDTH];
	float effectiveMass[CONSTRAINT_SIMD_WIDTH];

	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];
};

struct simd_distance_constraint
{
	simd_distance_constraint_batch* batches;
	uint32 numBatches;
};


// Ball-joint constraint.

struct ball_joint_constraint
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

struct simd_ball_joint_constraint_batch
{
	float relGlobalAnchorA[3][CONSTRAINT_SIMD_WIDTH];
	float relGlobalAnchorB[3][CONSTRAINT_SIMD_WIDTH];

	float bias[3][CONSTRAINT_SIMD_WIDTH];
	float invEffectiveMass[9][CONSTRAINT_SIMD_WIDTH];
	
	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];
};

struct simd_ball_joint_constraint
{
	simd_ball_joint_constraint_batch* batches;
	uint32 numBatches;
};


// Hinge-joint constraint.

struct hinge_joint_constraint
{
	vec3 localAnchorA;
	vec3 localAnchorB;
	vec3 localHingeAxisA;
	vec3 localHingeAxisB;

	// Used for limits and motor.
	vec3 localHingeTangentA;
	vec3 localHingeBitangentA;
	vec3 localHingeTangentB;

	// Limits. The rotation limits are the allowed deviations in radians from the initial relative rotation.
	// If the limits are not in the specified range, they are disabled.
	float minRotationLimit; // [-pi, 0]
	float maxRotationLimit; // [0, pi]

	// Motor.
	constraint_motor_type motorType;
	union
	{
		float motorVelocity;
		float motorTargetAngle;
	};
	float maxMotorTorque;
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
	float effectiveAxialMass; // Same for min and max limit and for motor.

	bool solveLimit;
	bool solveMotor;

	// Since at a single time, only one limit constraint can be violated, we only store this stuff once. 
	// 'limitSign' is positive for min- and negative for max-limit-violations.
	float limitImpulse;
	float limitBias;
	float limitSign;

	float motorImpulse;
	float maxMotorImpulse;
	float motorVelocity;

	vec3 motorAndLimitImpulseToAngularVelocityA;
	vec3 motorAndLimitImpulseToAngularVelocityB;
};


// Cone-twist constraint.

struct cone_twist_constraint
{
	vec3 localAnchorA;
	vec3 localAnchorB;

	float swingLimit;
	float twistLimit;

	vec3 localLimitAxisA;
	vec3 localLimitAxisB;

	vec3 localLimitTangentA;
	vec3 localLimitBitangentA;
	vec3 localLimitTangentB;

	// Motor.
	constraint_motor_type swingMotorType;
	union
	{
		float swingMotorVelocity;
		float swingMotorTargetAngle;
	};
	float maxSwingMotorTorque;
	float swingMotorAxis;

	constraint_motor_type twistMotorType;
	union
	{
		float twistMotorVelocity;
		float twistMotorTargetAngle;
	};
	float maxTwistMotorTorque;
};

struct cone_twist_constraint_update
{
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;
	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;

	vec3 bias;
	mat3 invEffectiveMass;

	bool solveSwingLimit;
	bool solveTwistLimit;
	bool solveSwingMotor;
	bool solveTwistMotor;

	// Limits.
	vec3 globalSwingAxis;
	float swingImpulse;
	float effectiveSwingLimitMass;
	float swingLimitBias;
	
	vec3 globalTwistAxis;
	float twistImpulse;
	float twistLimitSign;
	float effectiveTwistMass;
	float twistLimitBias;

	// Motors.
	float swingMotorImpulse;
	float maxSwingMotorImpulse;
	float swingMotorVelocity;
	vec3 globalSwingMotorAxis;
	float effectiveSwingMotorMass;

	float twistMotorImpulse;
	float maxTwistMotorImpulse;
	float twistMotorVelocity;

	vec3 twistMotorAndLimitImpulseToAngularVelocityA;
	vec3 twistMotorAndLimitImpulseToAngularVelocityB;

	vec3 swingMotorImpulseToAngularVelocityA;
	vec3 swingMotorImpulseToAngularVelocityB;
	vec3 swingLimitImpulseToAngularVelocityA;
	vec3 swingLimitImpulseToAngularVelocityB;
};



// Collision constraint.

struct collision_constraint
{
	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;
	vec3 tangent;

	vec3 tangentImpulseToAngularVelocityA;
	vec3 tangentImpulseToAngularVelocityB;
	vec3 normalImpulseToAngularVelocityA;
	vec3 normalImpulseToAngularVelocityB;

	float impulseInNormalDir;
	float impulseInTangentDir;
	float effectiveMassInNormalDir;
	float effectiveMassInTangentDir;
	float bias;
};

struct simd_collision_constraint_batch
{
	float relGlobalAnchorA[3][CONSTRAINT_SIMD_WIDTH];
	float relGlobalAnchorB[3][CONSTRAINT_SIMD_WIDTH];
	float normal[3][CONSTRAINT_SIMD_WIDTH];
	float tangent[3][CONSTRAINT_SIMD_WIDTH];

	float normalImpulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float tangentImpulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float normalImpulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];
	float tangentImpulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];

	float effectiveMassInNormalDir[CONSTRAINT_SIMD_WIDTH];
	float effectiveMassInTangentDir[CONSTRAINT_SIMD_WIDTH];
	float friction[CONSTRAINT_SIMD_WIDTH];
	float impulseInNormalDir[CONSTRAINT_SIMD_WIDTH];
	float impulseInTangentDir[CONSTRAINT_SIMD_WIDTH];
	float bias[CONSTRAINT_SIMD_WIDTH];

	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];
};

struct simd_collision_constraint
{
	simd_collision_constraint_batch* batches;
	uint32 numBatches;
};



struct rigid_body_global_state;
struct collision_contact;

void initializeDistanceVelocityConstraints(const rigid_body_global_state* rbs, const distance_constraint* input, const constraint_body_pair* bodyPairs, distance_constraint_update* output, uint32 count, float dt);
void solveDistanceVelocityConstraints(distance_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeBallJointVelocityConstraints(const rigid_body_global_state* rbs, const ball_joint_constraint* input, const constraint_body_pair* bodyPairs, ball_joint_constraint_update* output, uint32 count, float dt);
void solveBallJointVelocityConstraints(ball_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeHingeJointVelocityConstraints(const rigid_body_global_state* rbs, const hinge_joint_constraint* input, const constraint_body_pair* bodyPairs, hinge_joint_constraint_update* output, uint32 count, float dt);
void solveHingeJointVelocityConstraints(hinge_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeConeTwistVelocityConstraints(const rigid_body_global_state* rbs, const cone_twist_constraint* input, const constraint_body_pair* bodyPairs, cone_twist_constraint_update* output, uint32 count, float dt);
void solveConeTwistVelocityConstraints(cone_twist_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeCollisionVelocityConstraints(const rigid_body_global_state* rbs, const collision_contact* contacts, const constraint_body_pair* bodyPairs, collision_constraint* outConstraints, uint32 numContacts, float dt);
void solveCollisionVelocityConstraints(const collision_contact* contacts, collision_constraint* constraints, const constraint_body_pair* bodyPairs, uint32 count, rigid_body_global_state* rbs);




// SIMD.

void initializeDistanceVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const distance_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, simd_distance_constraint& outConstraints, float dt);
void solveDistanceVelocityConstraintsSIMD(simd_distance_constraint& constraints, rigid_body_global_state* rbs);

void initializeBallJointVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const ball_joint_constraint* input, const constraint_body_pair* bodyPairs, uint32 count,
	simd_ball_joint_constraint& outConstraints, float dt);
void solveBallJointVelocityConstraintsSIMD(simd_ball_joint_constraint& constraints, rigid_body_global_state* rbs);

void initializeCollisionVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const collision_contact* contacts, const constraint_body_pair* bodyPairs, uint32 numContacts,
	uint16 dummyRigidBodyIndex, simd_collision_constraint& outConstraints, float dt);
void solveCollisionVelocityConstraintsSIMD(simd_collision_constraint& constraints, rigid_body_global_state* rbs);

