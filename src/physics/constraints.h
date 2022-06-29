#pragma once

#include "core/math.h"
#include "core/memory.h"
#include "scene/scene.h"



struct rigid_body_global_state;
struct collision_contact;

#define CONSTRAINT_SIMD_WIDTH 8

enum constraint_type
{
	constraint_type_none = -1,

	constraint_type_distance,
	constraint_type_ball,
	constraint_type_fixed,
	constraint_type_hinge,
	constraint_type_cone_twist,
	constraint_type_slider,

	constraint_type_collision, // This is a bit of a special case, because it is generated in each frame.

	constraint_type_count,
};

#define INVALID_CONSTRAINT_EDGE UINT16_MAX

struct constraint_edge
{
	entity_handle constraintEntity;
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
	entity_handle entityA = entt::null;
	entity_handle entityB = entt::null;
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

struct distance_constraint_solver
{
	distance_constraint_update* constraints;
	uint32 count;
};

struct simd_distance_constraint_batch
{
	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];

	float relGlobalAnchorA[3][CONSTRAINT_SIMD_WIDTH];
	float relGlobalAnchorB[3][CONSTRAINT_SIMD_WIDTH];

	float impulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float impulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];

	float u[3][CONSTRAINT_SIMD_WIDTH];
	float bias[CONSTRAINT_SIMD_WIDTH];
	float effectiveMass[CONSTRAINT_SIMD_WIDTH];
};

struct simd_distance_constraint_solver
{
	simd_distance_constraint_batch* batches;
	uint32 numBatches;
};


// Ball constraint.

struct ball_constraint
{
	vec3 localAnchorA;
	vec3 localAnchorB;
};

struct ball_constraint_update
{
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;
	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;

	vec3 bias;
	mat3 invEffectiveMass;
};

struct ball_constraint_solver
{
	ball_constraint_update* constraints;
	uint32 count;
};

struct simd_ball_constraint_batch
{
	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];

	float relGlobalAnchorA[3][CONSTRAINT_SIMD_WIDTH];
	float relGlobalAnchorB[3][CONSTRAINT_SIMD_WIDTH];

	float bias[3][CONSTRAINT_SIMD_WIDTH];
	float invEffectiveMass[9][CONSTRAINT_SIMD_WIDTH];
};

struct simd_ball_constraint_solver
{
	simd_ball_constraint_batch* batches;
	uint32 numBatches;
};


// Fixed constraint.

struct fixed_constraint
{
	quat initialInvRotationDifference;

	vec3 localAnchorA;
	vec3 localAnchorB;
};

struct fixed_constraint_update
{
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;
	vec3 relGlobalAnchorA;
	vec3 relGlobalAnchorB;

	vec3 translationBias;
	mat3 invEffectiveTranslationMass;

	vec3 rotationBias;
	mat3 invEffectiveRotationMass;
};

struct fixed_constraint_solver
{
	fixed_constraint_update* constraints;
	uint32 count;
};

struct simd_fixed_constraint_batch
{
	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];

	float relGlobalAnchorA[3][CONSTRAINT_SIMD_WIDTH];
	float relGlobalAnchorB[3][CONSTRAINT_SIMD_WIDTH];

	float translationBias[3][CONSTRAINT_SIMD_WIDTH];
	float invEffectiveTranslationMass[9][CONSTRAINT_SIMD_WIDTH];

	float rotationBias[3][CONSTRAINT_SIMD_WIDTH];
	float invEffectiveRotationMass[9][CONSTRAINT_SIMD_WIDTH];
};

struct simd_fixed_constraint_solver
{
	simd_fixed_constraint_batch* batches;
	uint32 numBatches;
};


// Hinge constraint.

struct hinge_constraint
{
	vec3 localAnchorA;
	vec3 localAnchorB;
	vec3 localHingeAxisA;
	vec3 localHingeAxisB;

	// Limits. The rotation limits are the allowed deviations in radians from the initial relative rotation.
	// If the limits are not in the specified range, they are disabled.
	float minRotationLimit; // [-pi, 0]
	float maxRotationLimit; // [0, pi]

	// Motor.
	float maxMotorTorque;
	constraint_motor_type motorType;
	union
	{
		float motorVelocity;
		float motorTargetAngle;
	};


	// Used for limits and motor.
	vec3 localHingeTangentA;
	vec3 localHingeBitangentA;
	vec3 localHingeTangentB;
};

struct hinge_constraint_update
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

struct hinge_constraint_solver
{
	hinge_constraint_update* constraints;
	uint32 count;
};

struct simd_hinge_constraint_batch
{
	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];

	float relGlobalAnchorA[3][CONSTRAINT_SIMD_WIDTH];
	float relGlobalAnchorB[3][CONSTRAINT_SIMD_WIDTH];

	float translationBias[3][CONSTRAINT_SIMD_WIDTH];
	float invEffectiveTranslationMass[9][CONSTRAINT_SIMD_WIDTH];


	float rotationBias[2][CONSTRAINT_SIMD_WIDTH];
	float invEffectiveRotationMass[4][CONSTRAINT_SIMD_WIDTH];
	float bxa[3][CONSTRAINT_SIMD_WIDTH];
	float cxa[3][CONSTRAINT_SIMD_WIDTH];

	bool solveLimit;
	bool solveMotor;

	float globalRotationAxis[3][CONSTRAINT_SIMD_WIDTH];
	float effectiveLimitAxialMass[CONSTRAINT_SIMD_WIDTH];
	float effectiveMotorAxialMass[CONSTRAINT_SIMD_WIDTH];

	float limitImpulse[CONSTRAINT_SIMD_WIDTH];
	float limitBias[CONSTRAINT_SIMD_WIDTH];
	float limitSign[CONSTRAINT_SIMD_WIDTH];

	float motorImpulse[CONSTRAINT_SIMD_WIDTH];
	float maxMotorImpulse[CONSTRAINT_SIMD_WIDTH];
	float motorVelocity[CONSTRAINT_SIMD_WIDTH];

	float motorAndLimitImpulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float motorAndLimitImpulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];
};

struct simd_hinge_constraint_solver
{
	simd_hinge_constraint_batch* batches;
	uint32 numBatches;
};


// Cone-twist constraint.

struct cone_twist_constraint
{
	vec3 localAnchorA;
	vec3 localAnchorB;

	vec3 localLimitAxisA;
	vec3 localLimitAxisB;

	vec3 localLimitTangentA;
	vec3 localLimitBitangentA;
	vec3 localLimitTangentB;

	float swingLimit;
	float twistLimit;

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

struct cone_twist_constraint_solver
{
	cone_twist_constraint_update* constraints;
	uint32 count;
};

struct simd_cone_twist_constraint_batch
{
	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];

	float relGlobalAnchorA[3][CONSTRAINT_SIMD_WIDTH];
	float relGlobalAnchorB[3][CONSTRAINT_SIMD_WIDTH];

	float bias[3][CONSTRAINT_SIMD_WIDTH];
	float invEffectiveMass[9][CONSTRAINT_SIMD_WIDTH];

	bool solveSwingLimit;
	bool solveTwistLimit;
	bool solveSwingMotor;
	bool solveTwistMotor;

	// Limits.
	float globalSwingAxis[3][CONSTRAINT_SIMD_WIDTH];
	float swingImpulse[CONSTRAINT_SIMD_WIDTH];
	float effectiveSwingLimitMass[CONSTRAINT_SIMD_WIDTH];
	float swingLimitBias[CONSTRAINT_SIMD_WIDTH];

	float swingLimitImpulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float swingLimitImpulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];


	float globalTwistAxis[3][CONSTRAINT_SIMD_WIDTH];
	float twistImpulse[CONSTRAINT_SIMD_WIDTH];
	float twistLimitSign[CONSTRAINT_SIMD_WIDTH];
	float effectiveTwistLimitMass[CONSTRAINT_SIMD_WIDTH];
	float twistLimitBias[CONSTRAINT_SIMD_WIDTH];



	// Motors.
	float swingMotorImpulse[CONSTRAINT_SIMD_WIDTH];
	float maxSwingMotorImpulse[CONSTRAINT_SIMD_WIDTH];
	float swingMotorVelocity[CONSTRAINT_SIMD_WIDTH];
	float globalSwingMotorAxis[3][CONSTRAINT_SIMD_WIDTH];
	float effectiveSwingMotorMass[CONSTRAINT_SIMD_WIDTH];

	float swingMotorImpulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float swingMotorImpulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];

	float twistMotorImpulse[CONSTRAINT_SIMD_WIDTH];
	float maxTwistMotorImpulse[CONSTRAINT_SIMD_WIDTH];
	float twistMotorVelocity[CONSTRAINT_SIMD_WIDTH];
	float effectiveTwistMotorMass[CONSTRAINT_SIMD_WIDTH];

	float twistMotorAndLimitImpulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float twistMotorAndLimitImpulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];
};

struct simd_cone_twist_constraint_solver
{
	simd_cone_twist_constraint_batch* batches;
	uint32 numBatches;
};


// Slider constraint

struct slider_constraint
{
	quat initialInvRotationDifference;
	vec3 localAnchorA;
	vec3 localAnchorB;

	vec3 localAxisA;


	// Limit.
	float negDistanceLimit; // Body b cannot go farther to the "left" (negative slider axis) than this value. [-inf, 0] if active, positive if no limit.
	float posDistanceLimit; // Body b cannot go farther to the "right" (positive slider axis) than this value. [0, inf] if active, negative if no limit.

	// Motor.
	float maxMotorForce;
	constraint_motor_type motorType;
	union
	{
		float motorVelocity;
		float motorTargetDistance;
	};
};

struct slider_constraint_update
{
	uint16 rigidBodyIndexA;
	uint16 rigidBodyIndexB;

	vec3 rAuxt;
	vec3 rAuxb;
	vec3 rBxt;
	vec3 rBxb;

	vec3 tangent;
	vec3 bitangent;

	mat2 invEffectiveTranslationMass;
	vec2 translationBias;

	mat3 invEffectiveRotationMass;
	vec3 rotationBias;

	bool solveLimit;
	bool solveMotor;

	vec3 globalSliderAxis;
	float effectiveAxialMass;
	float limitBias;
	float limitImpulse;
	float limitSign;
	vec3 rAuxs;
	vec3 rBxs;

	vec3 limitImpulseToAngularVelocityA;
	vec3 limitImpulseToAngularVelocityB;

	float motorVelocity;
	float motorImpulse;
	float maxMotorImpulse;
};

struct slider_constraint_solver
{
	slider_constraint_update* constraints;
	uint32 count;
};

struct simd_slider_constraint_batch
{
	uint16 rbAIndices[CONSTRAINT_SIMD_WIDTH];
	uint16 rbBIndices[CONSTRAINT_SIMD_WIDTH];

	float rAuxt[3][CONSTRAINT_SIMD_WIDTH];
	float rAuxb[3][CONSTRAINT_SIMD_WIDTH];
	float rBxt[3][CONSTRAINT_SIMD_WIDTH];
	float rBxb[3][CONSTRAINT_SIMD_WIDTH];

	float tangent[3][CONSTRAINT_SIMD_WIDTH];
	float bitangent[3][CONSTRAINT_SIMD_WIDTH];

	float invEffectiveTranslationMass[4][CONSTRAINT_SIMD_WIDTH];
	float translationBias[2][CONSTRAINT_SIMD_WIDTH];

	float invEffectiveRotationMass[9][CONSTRAINT_SIMD_WIDTH];
	float rotationBias[3][CONSTRAINT_SIMD_WIDTH];

	float globalSliderAxis[3][CONSTRAINT_SIMD_WIDTH];

	bool solveLimit;
	bool solveMotor;

	float effectiveAxialMass[CONSTRAINT_SIMD_WIDTH];
	float limitBias[CONSTRAINT_SIMD_WIDTH];
	float limitImpulse[CONSTRAINT_SIMD_WIDTH];
	float limitSign[CONSTRAINT_SIMD_WIDTH];
	float rAuxs[3][CONSTRAINT_SIMD_WIDTH];
	float rBxs[3][CONSTRAINT_SIMD_WIDTH];

	float limitImpulseToAngularVelocityA[3][CONSTRAINT_SIMD_WIDTH];
	float limitImpulseToAngularVelocityB[3][CONSTRAINT_SIMD_WIDTH];

	float motorVelocity[CONSTRAINT_SIMD_WIDTH];
	float motorImpulse[CONSTRAINT_SIMD_WIDTH];
	float maxMotorImpulse[CONSTRAINT_SIMD_WIDTH];
	float effectiveMotorMass[CONSTRAINT_SIMD_WIDTH];
};

struct simd_slider_constraint_solver
{
	simd_slider_constraint_batch* batches;
	uint32 numBatches;
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

struct collision_constraint_solver
{
	collision_constraint* constraints;
	const collision_contact* contacts;
	const constraint_body_pair* bodyPairs;
	uint32 count;
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

struct simd_collision_constraint_solver
{
	simd_collision_constraint_batch* batches;
	uint32 numBatches;
};




distance_constraint_solver initializeDistanceVelocityConstraints(memory_arena& arena, const rigid_body_global_state* rbs, const distance_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveDistanceVelocityConstraints(distance_constraint_solver constraints, rigid_body_global_state* rbs);

ball_constraint_solver initializeBallVelocityConstraints(memory_arena& arena, const rigid_body_global_state* rbs, const ball_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveBallVelocityConstraints(ball_constraint_solver constraints, rigid_body_global_state* rbs);

fixed_constraint_solver initializeFixedVelocityConstraints(memory_arena& arena, const rigid_body_global_state* rbs, const fixed_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveFixedVelocityConstraints(fixed_constraint_solver constraints, rigid_body_global_state* rbs);

hinge_constraint_solver initializeHingeVelocityConstraints(memory_arena& arena, const rigid_body_global_state* rbs, const hinge_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveHingeVelocityConstraints(hinge_constraint_solver constraints, rigid_body_global_state* rbs);

cone_twist_constraint_solver initializeConeTwistVelocityConstraints(memory_arena& arena, const rigid_body_global_state* rbs, const cone_twist_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveConeTwistVelocityConstraints(cone_twist_constraint_solver constraints, rigid_body_global_state* rbs);

slider_constraint_solver initializeSliderVelocityConstraints(memory_arena& arena, const rigid_body_global_state* rbs, const slider_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveSliderVelocityConstraints(slider_constraint_solver constraints, rigid_body_global_state* rbs);

collision_constraint_solver initializeCollisionVelocityConstraints(memory_arena& arena, const rigid_body_global_state* rbs, const collision_contact* contacts, const constraint_body_pair* bodyPairs, uint32 numContacts, float dt);
void solveCollisionVelocityConstraints(collision_constraint_solver constraints, rigid_body_global_state* rbs);




// SIMD.

simd_distance_constraint_solver initializeDistanceVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const distance_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveDistanceVelocityConstraintsSIMD(simd_distance_constraint_solver constraints, rigid_body_global_state* rbs);

simd_ball_constraint_solver initializeBallVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const ball_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveBallVelocityConstraintsSIMD(simd_ball_constraint_solver constraints, rigid_body_global_state* rbs);

simd_fixed_constraint_solver initializeFixedVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const fixed_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveFixedVelocityConstraintsSIMD(simd_fixed_constraint_solver constraints, rigid_body_global_state* rbs);

simd_hinge_constraint_solver initializeHingeVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const hinge_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveHingeVelocityConstraintsSIMD(simd_hinge_constraint_solver constraints, rigid_body_global_state* rbs);

simd_cone_twist_constraint_solver initializeConeTwistVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const cone_twist_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveConeTwistVelocityConstraintsSIMD(simd_cone_twist_constraint_solver constraints, rigid_body_global_state* rbs);

simd_slider_constraint_solver initializeSliderVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const slider_constraint* input, const constraint_body_pair* bodyPairs, uint32 count, float dt);
void solveSliderVelocityConstraintsSIMD(simd_slider_constraint_solver constraints, rigid_body_global_state* rbs);

simd_collision_constraint_solver initializeCollisionVelocityConstraintsSIMD(memory_arena& arena, const rigid_body_global_state* rbs, const collision_contact* contacts, const constraint_body_pair* bodyPairs, uint32 numContacts, uint16 dummyRigidBodyIndex, float dt);
void solveCollisionVelocityConstraintsSIMD(simd_collision_constraint_solver constraints, rigid_body_global_state* rbs);



struct constraint_solver
{
	void initialize(memory_arena& arena, rigid_body_global_state* rbs,
		distance_constraint* distanceConstraints, constraint_body_pair* distanceConstraintBodyPairs, uint32 numDistanceConstraints,
		ball_constraint* ballConstraints, constraint_body_pair* ballConstraintBodyPairs, uint32 numBallConstraints,
		fixed_constraint* fixedConstraints, constraint_body_pair* fixedConstraintBodyPairs, uint32 numFixedConstraints,
		hinge_constraint* hingeConstraints, constraint_body_pair* hingeConstraintBodyPairs, uint32 numHingeConstraints,
		cone_twist_constraint* coneTwistConstraints, constraint_body_pair* coneTwistConstraintBodyPairs, uint32 numConeTwistConstraints,
		slider_constraint* sliderConstraints, constraint_body_pair* sliderConstraintBodyPairs, uint32 numSliderConstraints,
		collision_contact* contacts, constraint_body_pair* collisionBodyPairs, uint32 numContacts, 
		uint32 dummyRigidBodyIndex,	bool simd, float dt);

	void solveOneIteration();

private:

	rigid_body_global_state* rbs;
	bool simd;

	distance_constraint_solver distanceConstraintSolver;
	simd_distance_constraint_solver distanceConstraintSolverSIMD;

	ball_constraint_solver ballConstraintSolver;
	simd_ball_constraint_solver ballConstraintSolverSIMD;

	fixed_constraint_solver fixedConstraintSolver;
	simd_fixed_constraint_solver fixedConstraintSolverSIMD;

	hinge_constraint_solver hingeConstraintSolver;
	simd_hinge_constraint_solver hingeConstraintSolverSIMD;

	cone_twist_constraint_solver coneTwistConstraintSolver;
	simd_cone_twist_constraint_solver coneTwistConstraintSolverSIMD;

	slider_constraint_solver sliderConstraintSolver;
	simd_slider_constraint_solver sliderConstraintSolverSIMD;

	collision_constraint_solver collisionConstraintSolver;
	simd_collision_constraint_solver collisionConstraintSolverSIMD;
};

