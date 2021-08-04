#pragma once

#include "core/math.h"
#include "scene/scene.h"

enum constraint_type : uint16
{
	constraint_type_distance,
	constraint_type_ball_joint,
	constraint_type_hinge_joint,
	constraint_type_cone_twist,
};

#define INVALID_CONSTRAINT_EDGE UINT16_MAX

struct constraint_edge
{
	uint16 constraint;
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
};


// Cone-twist constraint.

struct cone_twist_constraint : physics_constraint
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
};



struct rigid_body_global_state;

void initializeDistanceVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const distance_constraint* input, distance_constraint_update* output, uint32 count, float dt);
void solveDistanceVelocityConstraints(distance_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeBallJointVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const ball_joint_constraint* input, ball_joint_constraint_update* output, uint32 count, float dt);
void solveBallJointVelocityConstraints(ball_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeHingeJointVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const hinge_joint_constraint* input, hinge_joint_constraint_update* output, uint32 count, float dt);
void solveHingeJointVelocityConstraints(hinge_joint_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);

void initializeConeTwistVelocityConstraints(scene& appScene, rigid_body_global_state* rbs, const cone_twist_constraint* input, cone_twist_constraint_update* output, uint32 count, float dt);
void solveConeTwistVelocityConstraints(cone_twist_constraint_update* constraints, uint32 count, rigid_body_global_state* rbs);
